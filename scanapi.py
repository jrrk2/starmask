#!/usr/bin/env python3

import os
import re
import sys
import argparse
from collections import defaultdict, namedtuple

# Define a structure to represent a function call with context
FunctionCall = namedtuple('FunctionCall', ['function', 'args', 'file', 'line', 'context', 'full_context'])

def extract_api_calls_with_context(file_path):
    """Extract API function calls with surrounding context to understand usage patterns."""
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
    except Exception as e:
        print(f"Error reading {file_path}: {e}")
        return []
    
    # Find all API function calls using the pattern (*API->Category->Function)(args)
    pattern = r'\(\*API->(\w+)->(\w+)\)\s*\((.*?)\)'
    
    calls = []
    for match in re.finditer(pattern, content):
        category = match.group(1)
        function = match.group(2)
        args_str = match.group(3).strip()
        
        # Calculate line number
        line_number = content[:match.start()].count('\n') + 1
        
        # Get a window of context (code around the call)
        start_context = max(0, match.start() - 200)
        end_context = min(len(content), match.end() + 200)
        context = content[start_context:end_context].strip()
        
        # Get the full statement containing the call
        # Find the line start
        line_start = content.rfind('\n', 0, match.start())
        if line_start == -1:
            line_start = 0
        else:
            line_start += 1
        
        # Find the line end (semicolon or line break)
        line_end = content.find(';', match.end())
        if line_end == -1:
            line_end = content.find('\n', match.end())
            if line_end == -1:
                line_end = len(content)
        else:
            line_end += 1  # Include the semicolon
        
        statement = content[line_start:line_end].strip()
        
        # Parse arguments (simple version)
        args = parse_arguments(args_str)
        
        calls.append(FunctionCall(
            f"{category}.{function}",
            args,
            file_path,
            line_number,
            statement,
            context
        ))
    
    return calls

def parse_arguments(args_str):
    """Parse a comma-separated list of arguments, handling nested parentheses."""
    if not args_str:
        return []
    
    args = []
    current_arg = ""
    paren_depth = 0
    
    for char in args_str:
        if char == ',' and paren_depth == 0:
            args.append(current_arg.strip())
            current_arg = ""
        else:
            if char == '(':
                paren_depth += 1
            elif char == ')':
                paren_depth = max(0, paren_depth - 1)  # Avoid negative values
            current_arg += char
    
    if current_arg:
        args.append(current_arg.strip())
    
    return args

def find_thread_calls(calls):
    """Filter for Thread-related calls."""
    return [call for call in calls if call.function.startswith('Thread.')]

def analyze_call_usage(calls):
    """Analyze how function calls are used in code (return checking, conditionals, etc.)."""
    usage_patterns = defaultdict(list)
    
    for call in calls:
        function = call.function
        context = call.context
        
        # Look for return value checks
        if "==" in context:
            pattern = re.escape(context.split('==')[0].strip()) + r'\s*==\s*(\w+)'
            match = re.search(pattern, context)
            if match:
                comparison_value = match.group(1)
                usage_patterns[function].append(f"return_check: == {comparison_value}")
        
        if "!=" in context:
            pattern = re.escape(context.split('!=')[0].strip()) + r'\s*!=\s*(\w+)'
            match = re.search(pattern, context)
            if match:
                comparison_value = match.group(1)
                usage_patterns[function].append(f"return_check: != {comparison_value}")
        
        # Look for if statements containing the call
        if context.lstrip().startswith('if'):
            usage_patterns[function].append("conditional: if statement")
        
        # Look for error handling
        if "throw" in context:
            usage_patterns[function].append("error_handling: throw exception")
        
        if "return" in context and not context.lstrip().startswith('return'):
            usage_patterns[function].append("error_handling: return on error")
        
        # Check for handle checking/validation before calls
        full_context_before = call.full_context[:call.full_context.find(context)]
        if "IsNull" in full_context_before and "if" in full_context_before:
            usage_patterns[function].append("validation: null check before call")
    
    return usage_patterns

def find_thread_handle_usage(calls):
    """Analyze how thread handles are used across different function calls."""
    # Try to identify variables that hold thread handles
    handle_vars = set()
    handle_patterns = []
    
    for call in calls:
        if call.function == 'Thread.CreateThread':
            # Look for assignments of the return value
            match = re.search(r'(\w+)\s*=\s*\(\*API->Thread->CreateThread\)', call.full_context)
            if match:
                handle_vars.add(match.group(1))
                handle_patterns.append(f"CreateThread return stored in: {match.group(1)}")
        
        # Look for thread handles in other thread function calls
        if call.args and call.function.startswith('Thread.'):
            first_arg = call.args[0].strip()
            # If it's a variable and not a literal
            if first_arg and not first_arg.startswith('0x') and first_arg != 'nullptr' and first_arg != 'NULL':
                if first_arg in handle_vars:
                    handle_patterns.append(f"Handle variable {first_arg} used in: {call.function}")
    
    return handle_patterns

def analyze_thread_dispatcher_usage(calls):
    """Analyze how thread dispatchers are used."""
    dispatcher_patterns = []
    
    # Look for SetThreadExecRoutine calls
    for call in calls:
        if call.function == 'Thread.SetThreadExecRoutine' and len(call.args) >= 2:
            handle = call.args[0].strip()
            dispatcher = call.args[1].strip()
            
            dispatcher_patterns.append(f"SetThreadExecRoutine({handle}, {dispatcher})")
            
            # Look for any code that might be defining the dispatcher function
            for other_call in calls:
                if dispatcher in other_call.full_context and "static void" in other_call.full_context:
                    context_lines = other_call.full_context.split('\n')
                    for i, line in enumerate(context_lines):
                        if dispatcher in line and "static void" in line:
                            # Try to extract the function signature
                            signature = line.strip()
                            # Look for the function body
                            j = i + 1
                            while j < len(context_lines) and '{' not in context_lines[j]:
                                signature += ' ' + context_lines[j].strip()
                                j += 1
                            
                            dispatcher_patterns.append(f"Dispatcher definition: {signature}")
                            break
    
    return dispatcher_patterns

def extract_thread_related_types(directory):
    """Extract thread-related type definitions and constants."""
    thread_types = []
    
    # Common thread-related types and constants
    type_patterns = [
        r'typedef\s+([^;]+)\s+thread_handle\s*;',
        r'typedef\s+([^;]+)\s+thread_priority\s*;',
        r'#define\s+THREAD_\w+\s+([^/\n]+)',
        r'const\s+\w+\s+THREAD_\w+\s*=\s*([^;]+);'
    ]
    
    for root, _, files in os.walk(directory):
        for file in files:
            if not file.endswith(('.h', '.hpp')):
                continue
            
            file_path = os.path.join(root, file)
            try:
                with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()
                
                # Look for thread-related type definitions
                for pattern in type_patterns:
                    for match in re.finditer(pattern, content):
                        thread_types.append((match.group(0).strip(), os.path.basename(file_path)))
                
                # Look for thread status flags
                status_pattern = r'(THREAD_[A-Z_]+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)'
                for match in re.finditer(status_pattern, content):
                    flag_name = match.group(1)
                    flag_value = match.group(2)
                    thread_types.append((f"{flag_name} = {flag_value}", os.path.basename(file_path)))
            
            except Exception as e:
                print(f"Error processing {file_path}: {e}")
    
    return thread_types

def scan_directory(directory):
    """Scan a directory recursively for PCL source files."""
    all_calls = []
    
    for root, _, files in os.walk(directory):
        for file in files:
            # Skip non-source files
            if not file.endswith(('.cpp', '.c', '.cc', '.cxx')):
                continue
            
            file_path = os.path.join(root, file)
            
            # Extract API calls from the file
            calls = extract_api_calls_with_context(file_path)
            all_calls.extend(calls)
    
    # Filter for Thread-related calls
    thread_calls = find_thread_calls(all_calls)
    
    # Analyze usage patterns
    usage_patterns = analyze_call_usage(thread_calls)
    
    # Analyze thread handle usage
    handle_patterns = find_thread_handle_usage(thread_calls)
    
    # Analyze thread dispatcher usage
    dispatcher_patterns = analyze_thread_dispatcher_usage(thread_calls)
    
    # Extract thread-related types
    thread_types = extract_thread_related_types(directory)
    
    return thread_calls, usage_patterns, handle_patterns, dispatcher_patterns, thread_types

def generate_thread_mock_code(calls, usage_patterns, thread_types):
    """Generate C++ code for thread function mocks based on observed patterns."""
    # Count call frequencies
    call_counts = defaultdict(int)
    for call in calls:
        call_counts[call.function] += 1
    
    # Sort functions by frequency
    sorted_functions = sorted(call_counts.items(), key=lambda x: x[1], reverse=True)
    
    code = []
    code.append("// Generated thread function mocks based on observed patterns")
    code.append("#include <iostream>")
    code.append("#include <map>")
    code.append("#include <mutex>")
    code.append("#include <string>")
    code.append("#include <pthread.h>")
    code.append("")
    code.append("namespace pcl_mock {")
    code.append("")
    
    # Add thread-related types
    code.append("// Thread-related types and constants")
    code.append("typedef int api_bool;")
    code.append("const api_bool api_true = 1;")
    code.append("const api_bool api_false = 0;")
    code.append("typedef unsigned int uint32;")
    code.append("typedef void* api_handle;")
    code.append("typedef void* thread_handle;")
    
    # Add thread status flags
    code.append("")
    code.append("// Thread status flags")
    status_flags_found = False
    for type_def, source_file in thread_types:
        if "THREAD_" in type_def and "=" in type_def:
            code.append(f"// From {source_file}")
            code.append(f"const uint32 {type_def};")
            status_flags_found = True
    
    # Add default status flags if none were found
    if not status_flags_found:
        code.append("// Default thread status flags")
        code.append("const uint32 THREAD_RUNNING = 0x80000000;")
        code.append("const uint32 THREAD_FINISHED = 0x40000000;")
        code.append("const uint32 THREAD_CANCELED = 0x20000000;")
    
    code.append("")
    
    # Thread data structure
    code.append("// Thread data structure")
    code.append("struct ThreadData {")
    code.append("    pthread_t thread;")
    code.append("    void* thread_object;  // Points to the PCL Thread object")
    code.append("    void (*dispatcher)(void*);")
    code.append("    uint32 status;")
    code.append("    int priority;")
    code.append("    pthread_mutex_t mutex;")
    code.append("    pthread_cond_t condition;")
    code.append("    bool started;")
    code.append("    bool active;")
    code.append("    std::string console_output;")
    code.append("")
    code.append("    ThreadData() : thread(), thread_object(nullptr), dispatcher(nullptr),")
    code.append("                  status(0), priority(0), started(false), active(false) {")
    code.append("        pthread_mutex_init(&mutex, nullptr);")
    code.append("        pthread_cond_init(&condition, nullptr);")
    code.append("    }")
    code.append("")
    code.append("    ~ThreadData() {")
    code.append("        pthread_mutex_destroy(&mutex);")
    code.append("        pthread_cond_destroy(&condition);")
    code.append("    }")
    code.append("};")
    code.append("")
    
    # Global storage
    code.append("// Global storage for thread data")
    code.append("static std::map<void*, ThreadData*> g_thread_data;")
    code.append("static std::mutex g_thread_mutex;")
    code.append("")
    
    # Helper functions
    code.append("// Get thread data by handle")
    code.append("static ThreadData* get_thread_data(void* handle) {")
    code.append("    std::lock_guard<std::mutex> lock(g_thread_mutex);")
    code.append("    auto it = g_thread_data.find(handle);")
    code.append("    if (it != g_thread_data.end()) {")
    code.append("        return it->second;")
    code.append("    }")
    code.append("    return nullptr;")
    code.append("}")
    code.append("")
    
    # Thread wrapper function
    code.append("// Thread wrapper function")
    code.append("static void* thread_wrapper(void* arg) {")
    code.append("    ThreadData* data = static_cast<ThreadData*>(arg);")
    code.append("    ")
    code.append("    // Mark thread as active")
    code.append("    pthread_mutex_lock(&data->mutex);")
    code.append("    data->active = true;")
    code.append("    data->status |= THREAD_RUNNING;")
    code.append("    pthread_mutex_unlock(&data->mutex);")
    code.append("    ")
    code.append("    // Call the dispatcher with the thread object itself")
    code.append("    try {")
    code.append("        if (data->dispatcher) {")
    code.append("            data->dispatcher(data->thread_object);")
    code.append("        } else {")
    code.append("            std::cerr << \"Warning: No dispatcher set for thread\" << std::endl;")
    code.append("        }")
    code.append("    } catch (...) {")
    code.append("        // PCL explicitly catches all exceptions in thread dispatchers")
    code.append("        std::cerr << \"Caught exception in thread\" << std::endl;")
    code.append("    }")
    code.append("    ")
    code.append("    // Mark thread as finished")
    code.append("    pthread_mutex_lock(&data->mutex);")
    code.append("    data->active = false;")
    code.append("    data->status &= ~THREAD_RUNNING;")
    code.append("    data->status |= THREAD_FINISHED;")
    code.append("    pthread_cond_broadcast(&data->condition);")
    code.append("    pthread_mutex_unlock(&data->mutex);")
    code.append("    ")
    code.append("    return nullptr;")
    code.append("}")
    code.append("")
    
    # Generate mock functions for each observed Thread function
    for func, count in sorted_functions:
       # Only handle Thread functions
       if not func.startswith('Thread.'):
            continue
        
       _, name = func.split('.')
        
       # Special handling for key functions
       if name == 'CreateThread':
            code.append("// Mock for CreateThread")
            code.append("// Called frequently in PCL: found in " + str(count) + " places")
            
            # Add observed usage patterns as comments
            for pattern in usage_patterns.get(func, []):
                code.append(f"// Usage pattern: {pattern}")
            
            code.append("void* CreateThread(void* module_handle, void* thread_object, int flags) {")
            code.append("    std::cout << \"CreateThread called with thread_object=\" << thread_object << std::endl;")
            code.append("    ")
            code.append("    // Create thread data structure")
            code.append("    ThreadData* data = new ThreadData();")
            code.append("    data->thread_object = thread_object;")
            code.append("    ")
            code.append("    // In PCL, the thread handle must be the Thread object itself")
            code.append("    void* handle = thread_object;")
            code.append("    ")
            code.append("    // Store thread data using the thread object as the key")
            code.append("    {")
            code.append("        std::lock_guard<std::mutex> lock(g_thread_mutex);")
            code.append("        g_thread_data[handle] = data;")
            code.append("    }")
            code.append("    ")
            code.append("    return handle;")
            code.append("}")
            code.append("")
        
       elif name == 'SetThreadExecRoutine':
            code.append("// Mock for SetThreadExecRoutine")
            code.append("// Called frequently in PCL: found in " + str(count) + " places")
            
            for pattern in usage_patterns.get(func, []):
                code.append(f"// Usage pattern: {pattern}")
            
            code.append("void SetThreadExecRoutine(void* thread_handle, void* dispatcher) {")
            code.append("    std::cout << \"SetThreadExecRoutine called with dispatcher=\" << dispatcher << std::endl;")
            code.append("    ")
            code.append("    ThreadData* data = get_thread_data(thread_handle);")
            code.append("    if (data) {")
            code.append("        pthread_mutex_lock(&data->mutex);")
            code.append("        data->dispatcher = reinterpret_cast<void (*)(void*)>(dispatcher);")
            code.append("        pthread_mutex_unlock(&data->mutex);")
            code.append("    } else {")
            code.append("        std::cerr << \"Error: No thread data found for handle\" << std::endl;")
            code.append("    }")
            code.append("}")
            code.append("")
        
       elif name == 'StartThread':
            code.append("// Mock for StartThread")
            code.append("// Called frequently in PCL: found in " + str(count) + " places")
            
            for pattern in usage_patterns.get(func, []):
                code.append(f"// Usage pattern: {pattern}")
            
            code.append("int StartThread(void* thread_handle, int priority) {")
            code.append("    std::cout << \"StartThread called with priority=\" << priority << std::endl;")
            code.append("    ")
            code.append("    ThreadData* data = get_thread_data(thread_handle);")
            code.append("    if (!data) {")
            code.append("        std::cerr << \"Error: No thread data found for handle\" << std::endl;")
            code.append("        return api_false;")
            code.append("    }")
            code.append("    ")
            code.append("    pthread_mutex_lock(&data->mutex);")
            code.append("    ")
            code.append("    // Check if thread is already started")
            code.append("    if (data->started) {")
            code.append("        pthread_mutex_unlock(&data->mutex);")
            code.append("        return api_false;")
            code.append("    }")
            code.append("    ")
            code.append("    data->priority = priority;")
            code.append("    data->started = true;")
            code.append("    ")
            code.append("    // Create the actual thread")
            code.append("    int result = pthread_create(&data->thread, nullptr, thread_wrapper, data);")
            code.append("    ")
            code.append("    pthread_mutex_unlock(&data->mutex);")
            code.append("    ")
            code.append("    return (result == 0) ? api_true : api_false;")
            code.append("}")
            code.append("")
        
       elif name == 'IsThreadActive':
            code.append("// Mock for IsThreadActive")
            code.append("// Called frequently in PCL: found in " + str(count) + " places")
            
            for pattern in usage_patterns.get(func, []):
                code.append(f"// Usage pattern: {pattern}")
            
            code.append("int IsThreadActive(void* thread_handle) {")
            code.append("    ThreadData* data = get_thread_data(thread_handle);")
            code.append("    if (!data) {")
            code.append("        return api_false;")
            code.append("    }")
            code.append("    ")
            code.append("    pthread_mutex_lock(&data->mutex);")
            code.append("    bool active = data->active;")
            code.append("    pthread_mutex_unlock(&data->mutex);")
            code.append("    ")
            code.append("    return active ? api_true : api_false;")
            code.append("}")
            code.append("")
        
       elif name == 'GetThreadStatus':
            code.append("// Mock for GetThreadStatus")
            code.append("// Called frequently in PCL: found in " + str(count) + " places")
            
            for pattern in usage_patterns.get(func, []):
                code.append(f"// Usage pattern: {pattern}")
            
            code.append("uint32 GetThreadStatus(void* thread_handle) {")
            code.append("    ThreadData* data = get_thread_data(thread_handle);")
            code.append("    if (!data) {")
            code.append("        return 0;")
            code.append("    }")
            code.append("    ")
            code.append("    pthread_mutex_lock(&data->mutex);")
            code.append("    uint32 status = data->status;")
            code.append("    pthread_mutex_unlock(&data->mutex);")
            code.append("    ")
            code.append("    return status;")
            code.append("}")
            code.append("")
        
       elif name == 'GetThreadStatusEx':
            code.append("// Mock for GetThreadStatusEx")
            code.append("// Called frequently in PCL: found in " + str(count) + " places")
            
            for pattern in usage_patterns.get(func, []):
                code.append(f"// Usage pattern: {pattern}")
            
            code.append("int GetThreadStatusEx(void* thread_handle, uint32* status, int flags) {")
            code.append("    ThreadData* data = get_thread_data(thread_handle);")
            code.append("    if (!data) {")
            code.append("        return api_false;")
            code.append("    }")
            code.append("    ")
            code.append("    pthread_mutex_lock(&data->mutex);")
            code.append("    *status = data->status;")
            code.append("    pthread_mutex_unlock(&data->mutex);")
            code.append("    ")
            code.append("    return api_true;")
            code.append("}")
            code.append("")
        
       elif name == 'WaitThread':
            code.append("// Mock for WaitThread")
            code.append("// Called frequently in PCL: found in " + str(count) + " places")
            
            for pattern in usage_patterns.get(func, []):
                code.append(f"// Usage pattern: {pattern}")
            
            code.append("int WaitThread(void* thread_handle, uint32 timeout_ms) {")
            code.append("    ThreadData* data = get_thread_data(thread_handle);")
            code.append("    if (!data) {")
            code.append("        return api_false;")
            code.append("    }")
            code.append("    ")
            code.append("    pthread_mutex_lock(&data->mutex);")
            code.append("    ")
            code.append("    if (!data->active) {")
            code.append("        // Thread already finished")
            code.append("        pthread_mutex_unlock(&data->mutex);")
            code.append("        return api_true;")
            code.append("    }")
            code.append("    ")
            code.append("    if (timeout_ms == 0xFFFFFFFF) {")
            code.append("        // Wait indefinitely")
            code.append("        while (data->active) {")
            code.append("            pthread_cond_wait(&data->condition, &data->mutex);")
            code.append("        }")
            code.append("        pthread_mutex_unlock(&data->mutex);")
            code.append("        return api_true;")
            code.append("    } else {")
            code.append("        // Wait with timeout")
            code.append("        struct timespec ts;")
            code.append("        clock_gettime(CLOCK_REALTIME, &ts);")
            code.append("        ts.tv_sec += timeout_ms / 1000;")
            code.append("        ts.tv_nsec += (timeout_ms % 1000) * 1000000;")
            code.append("        if (ts.tv_nsec >= 1000000000) {")
            code.append("            ts.tv_sec += 1;")
            code.append("            ts.tv_nsec -= 1000000000;")
            code.append("        }")
            code.append("        ")
            code.append("        int result = 0;")
            code.append("        while (data->active && result == 0) {")
            code.append("            result = pthread_cond_timedwait(&data->condition, &data->mutex, &ts);")
            code.append("        }")
            code.append("        ")
            code.append("        bool finished = !data->active;")
            code.append("        pthread_mutex_unlock(&data->mutex);")
            code.append("        return finished ? api_true : api_false;")
            code.append("    }")
            code.append("}")
            code.append("")
        
       elif name == 'KillThread':
            code.append("// Mock for KillThread")
            code.append("// Called frequently in PCL: found in " + str(count) + " places")
            
            for pattern in usage_patterns.get(func, []):
                code.append(f"// Usage pattern: {pattern}")
            
            code.append("void KillThread(void* thread_handle) {")
            code.append("    ThreadData* data = get_thread_data(thread_handle);")
            code.append("    if (!data) {")
            code.append("        return;")
            code.append("    }")
            code.append("    ")
            code.append("    pthread_mutex_lock(&data->mutex);")
            code.append("    if (data->active) {")
            code.append("        pthread_cancel(data->thread);")
            code.append("        data->active = false;")
            code.append("        data->status &= ~THREAD_RUNNING;")
            code.append("        data->status |= THREAD_CANCELED;")
            code.append("        pthread_cond_broadcast(&data->condition);")
            code.append("    }")
            code.append("    pthread_mutex_unlock(&data->mutex);")
            code.append("}")
            code.append("")
        
       elif name == 'GetCurrentThread':
            code.append("// Mock for GetCurrentThread")
            code.append("// Called frequently in PCL: found in " + str(count) + " places")
            
            for pattern in usage_patterns.get(func, []):
                code.append(f"// Usage pattern: {pattern}")
            
            code.append("void* GetCurrentThread() {")
            code.append("    pthread_t current = pthread_self();")
            code.append("    ")
            code.append("    // Find the handle for the current thread")
            code.append("    std::lock_guard<std::mutex> lock(g_thread_mutex);")
            code.append("    for (auto& pair : g_thread_data) {")
            code.append("        if (pthread_equal(pair.second->thread, current)) {")
            code.append("            return pair.first;")
            code.append("        }")
            code.append("    }")
            code.append("    ")
            code.append("    // Not found, return null (represents root thread in PCL)")
            code.append("    return nullptr;")
            code.append("}")
            code.append("")
        
       elif name == 'SleepThread':
            code.append("// Mock for SleepThread")
            code.append("// Called frequently in PCL: found in " + str(count) + " places")
            
            for pattern in usage_patterns.get(func, []):
                code.append(f"// Usage pattern: {pattern}")
            
            code.append("void SleepThread(void* thread_handle, uint32 ms) {")
            code.append("    // Simple implementation - just sleep")
            code.append("    usleep(ms * 1000);  // Convert to microseconds")
            code.append("}")
            code.append("")
        
       elif name == 'SetThreadStatus':
           code.append("// Mock for SetThreadStatus")
           code.append("// Called frequently in PCL: found in " + str(count) + " places")
            
           for pattern in usage_patterns.get(func, []):
                code.append(f"// Usage pattern: {pattern}")
            
           code.append("void SetThreadStatus(void* thread_handle, uint32 status) {")
           code.append("    ThreadData* data = get_thread_data(thread_handle);")
           code.append("void SetThreadStatus(void* thread_handle, uint32 status) {")
           code.append("    ThreadData* data = get_thread_data(thread_handle);")
           code.append("    if (data) {")
           code.append("        pthread_mutex_lock(&data->mutex);")
           code.append("        data->status = status;")
           code.append("        pthread_mutex_unlock(&data->mutex);")
           code.append("    }")
           code.append("}")
           code.append("")
       
       elif name == 'GetThreadPriority':
           code.append("// Mock for GetThreadPriority")
           code.append("// Called frequently in PCL: found in " + str(count) + " places")
           
           for pattern in usage_patterns.get(func, []):
               code.append(f"// Usage pattern: {pattern}")
           
           code.append("int GetThreadPriority(void* thread_handle) {")
           code.append("    ThreadData* data = get_thread_data(thread_handle);")
           code.append("    if (!data) {")
           code.append("        return 0;  // Default priority")
           code.append("    }")
           code.append("    ")
           code.append("    pthread_mutex_lock(&data->mutex);")
           code.append("    int priority = data->priority;")
           code.append("    pthread_mutex_unlock(&data->mutex);")
           code.append("    ")
           code.append("    return priority;")
           code.append("}")
           code.append("")
       
       elif name == 'SetThreadPriority':
           code.append("// Mock for SetThreadPriority")
           code.append("// Called frequently in PCL: found in " + str(count) + " places")
           
           for pattern in usage_patterns.get(func, []):
               code.append(f"// Usage pattern: {pattern}")
           
           code.append("void SetThreadPriority(void* thread_handle, int priority) {")
           code.append("    ThreadData* data = get_thread_data(thread_handle);")
           code.append("    if (data) {")
           code.append("        pthread_mutex_lock(&data->mutex);")
           code.append("        data->priority = priority;")
           code.append("        pthread_mutex_unlock(&data->mutex);")
           code.append("        ")
           code.append("        // On real systems, you'd use pthread_setschedparam here")
           code.append("    }")
           code.append("}")
           code.append("")
       
       elif name == 'GetThreadStackSize' or name == 'SetThreadStackSize':
           if name == 'GetThreadStackSize':
               code.append("// Mock for GetThreadStackSize")
               code.append("// Called in PCL: found in " + str(count) + " places")
               
               for pattern in usage_patterns.get(func, []):
                   code.append(f"// Usage pattern: {pattern}")
               
               code.append("int GetThreadStackSize(void* thread_handle) {")
               code.append("    // Return a reasonable default stack size")
               code.append("    return 8 * 1024 * 1024;  // 8MB")
               code.append("}")
               code.append("")
           else:
               code.append("// Mock for SetThreadStackSize")
               code.append("// Called in PCL: found in " + str(count) + " places")
               
               for pattern in usage_patterns.get(func, []):
                   code.append(f"// Usage pattern: {pattern}")
               
               code.append("void SetThreadStackSize(void* thread_handle, int stack_size) {")
               code.append("    // In a real implementation, you'd store this in ThreadData")
               code.append("    // and use it when creating the thread")
               code.append("}")
               code.append("")
       
       elif name == 'PerformanceAnalysisValue':
           code.append("// Mock for PerformanceAnalysisValue")
           code.append("// Called in PCL: found in " + str(count) + " places")
           
           for pattern in usage_patterns.get(func, []):
               code.append(f"// Usage pattern: {pattern}")
           
           code.append("int PerformanceAnalysisValue(int algorithm, size_t length, size_t item_size, ")
           code.append("                          int floating_point, int kernel_size, int width, int height) {")
           code.append("    // Simple implementation - suggest using half the available CPU cores")
           code.append("    int cpu_count = 4;  // Assume 4 cores")
           code.append("    return std::max(1, cpu_count / 2);")
           code.append("}")
           code.append("")
       
       # Console output functions
       elif name.startswith('AppendThreadConsoleOutputText') or name.startswith('ClearThreadConsoleOutputText') or name.startswith('GetThreadConsoleOutputText'):
           if name == 'AppendThreadConsoleOutputText':
               code.append("// Mock for AppendThreadConsoleOutputText")
               code.append("// Called in PCL: found in " + str(count) + " places")
               
               for pattern in usage_patterns.get(func, []):
                   code.append(f"// Usage pattern: {pattern}")
               
               code.append("void AppendThreadConsoleOutputText(void* thread_handle, const char* text) {")
               code.append("    ThreadData* data = get_thread_data(thread_handle);")
               code.append("    if (data) {")
               code.append("        pthread_mutex_lock(&data->mutex);")
               code.append("        data->console_output += text;")
               code.append("        pthread_mutex_unlock(&data->mutex);")
               code.append("    }")
               code.append("}")
               code.append("")
           elif name == 'ClearThreadConsoleOutputText':
               code.append("// Mock for ClearThreadConsoleOutputText")
               code.append("// Called in PCL: found in " + str(count) + " places")
               
               for pattern in usage_patterns.get(func, []):
                   code.append(f"// Usage pattern: {pattern}")
               
               code.append("void ClearThreadConsoleOutputText(void* thread_handle) {")
               code.append("    ThreadData* data = get_thread_data(thread_handle);")
               code.append("    if (data) {")
               code.append("        pthread_mutex_lock(&data->mutex);")
               code.append("        data->console_output.clear();")
               code.append("        pthread_mutex_unlock(&data->mutex);")
               code.append("    }")
               code.append("}")
               code.append("")
           else:  # GetThreadConsoleOutputText
               code.append("// Mock for GetThreadConsoleOutputText")
               code.append("// Called in PCL: found in " + str(count) + " places")
               
               for pattern in usage_patterns.get(func, []):
                   code.append(f"// Usage pattern: {pattern}")
               
               code.append("int GetThreadConsoleOutputText(void* thread_handle, char* buffer, size_t* size) {")
               code.append("    ThreadData* data = get_thread_data(thread_handle);")
               code.append("    if (!data) {")
               code.append("        return api_false;")
               code.append("    }")
               code.append("    ")
               code.append("    pthread_mutex_lock(&data->mutex);")
               code.append("    ")
               code.append("    if (buffer == nullptr) {")
               code.append("        // Just return the size")
               code.append("        *size = data->console_output.size() + 1;  // +1 for null terminator")
               code.append("    } else {")
               code.append("        // Copy data to buffer")
               code.append("        size_t to_copy = std::min(*size - 1, data->console_output.size());")
               code.append("        if (to_copy > 0) {")
               code.append("            memcpy(buffer, data->console_output.c_str(), to_copy);")
               code.append("        }")
               code.append("        buffer[to_copy] = '\\0';  // Null terminate")
               code.append("        *size = to_copy + 1;")
               code.append("    }")
               code.append("    ")
               code.append("    pthread_mutex_unlock(&data->mutex);")
               code.append("    return api_true;")
               code.append("}")
               code.append("")
       
       else:
           # Generic implementation for other functions
           code.append(f"// Mock for {name}")
           code.append(f"// Called in PCL: found in {count} places")
           
           for pattern in usage_patterns.get(func, []):
               code.append(f"// Usage pattern: {pattern}")
           
           # Different implementations based on common function name patterns
           if name.startswith('Is'):
               code.append(f"int {name}(void* thread_handle) {{")
               code.append(f"    std::cout << \"{name} called with thread_handle=\" << thread_handle << std::endl;")
               code.append("    // Default implementation returns true")
               code.append("    return api_true;")
               code.append("}")
           elif name.startswith('Get'):
               code.append(f"// TODO: Implement {name} with proper return type")
               code.append(f"void* {name}(void* thread_handle) {{")
               code.append(f"    std::cout << \"{name} called with thread_handle=\" << thread_handle << std::endl;")
               code.append("    // Default implementation")
               code.append("    return nullptr;")
               code.append("}")
           elif name.startswith('Set'):
               code.append(f"void {name}(void* thread_handle, void* value) {{")
               code.append(f"    std::cout << \"{name} called with thread_handle=\" << thread_handle")
               code.append("              << \", value=\" << value << std::endl;")
               code.append("    // Default empty implementation")
               code.append("}")
           else:
               code.append(f"void {name}(void* thread_handle) {{")
               code.append(f"    std::cout << \"{name} called with thread_handle=\" << thread_handle << std::endl;")
               code.append("    // Default empty implementation")
               code.append("}")
           
           code.append("")
   
    # Function registration
    code.append("// Register all thread functions")
    code.append("void RegisterThreadFunctions(void (*RegisterFunction)(const char*, void*)) {")
    for func, _ in sorted_functions:
        if func.startswith('Thread.'):
            _, name = func.split('.')
            code.append(f"    RegisterFunction(\"Thread/{name}\", (void*){name});")
    code.append("}")
    code.append("")

    code.append("} // namespace pcl_mock")

    return "\n".join(code)

def main():
    parser = argparse.ArgumentParser(description='Analyze PCL thread function usage patterns')
    parser.add_argument('directory', help='Path to PCL source directory')
    parser.add_argument('--output', '-o', help='Output file for generated code')
    args = parser.parse_args()

    if not os.path.isdir(args.directory):
        print(f"Error: {args.directory} is not a valid directory")
        return 1

    print(f"Scanning PCL source code in {args.directory}...")
    thread_calls, usage_patterns, handle_patterns, dispatcher_patterns, thread_types = scan_directory(args.directory)

    print(f"\nFound {len(thread_calls)} thread-related function calls")

    # Count call frequencies
    call_counts = defaultdict(int)
    for call in thread_calls:
        call_counts[call.function] += 1

    # Print most common thread functions
    print("\n===== Most Common Thread Functions =====")
    for func, count in sorted(call_counts.items(), key=lambda x: x[1], reverse=True)[:15]:
        print(f"{func}: {count} calls")

    # Print usage patterns
    print("\n===== Function Usage Patterns =====")
    for func, patterns in sorted(usage_patterns.items()):
        if patterns and func.startswith('Thread.'):
            print(f"\n{func}:")
            for pattern in patterns[:5]:  # Limit to 5 patterns per function
                print(f"  {pattern}")
            if len(patterns) > 5:
                print(f"  ... and {len(patterns) - 5} more patterns")

    # Print thread handle usage patterns
    print("\n===== Thread Handle Usage Patterns =====")
    for pattern in handle_patterns[:10]:  # Limit to 10 patterns
        print(f"  {pattern}")
    if len(handle_patterns) > 10:
        print(f"  ... and {len(handle_patterns) - 10} more patterns")

    # Print thread dispatcher patterns
    print("\n===== Thread Dispatcher Patterns =====")
    for pattern in dispatcher_patterns:
        print(f"  {pattern}")

    # Print thread-related types
    print("\n===== Thread-Related Types =====")
    for type_def, source_file in thread_types[:10]:  # Limit to 10 types
        print(f"  {type_def} (from {source_file})")
    if len(thread_types) > 10:
        print(f"  ... and {len(thread_types) - 10} more types")

    # Generate mock code
    code = generate_thread_mock_code(thread_calls, usage_patterns, thread_types)

    # Write code to file or print to console
    if args.output:
        with open(args.output, 'w') as f:
            f.write(code)
        print(f"\nGenerated mock code written to {args.output}")
    else:
        print("\n===== Generated Thread Mock Code =====")
        print(code)

    return 0

if __name__ == "__main__":
    sys.exit(main())
