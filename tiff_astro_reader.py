#!/usr/bin/env python3
"""
TIFF Astronomical Metadata Reader
Extracts Origin astrophotography metadata from TIFF files
"""

import struct
import json
import sys
from typing import Optional, Dict, Any

class TIFFAstroReader:
    def __init__(self, filename: str):
        self.filename = filename
        self.is_little_endian = True
        
    def read_uint16(self, f) -> int:
        data = f.read(2)
        if len(data) != 2:
            return 0
        return struct.unpack('<H' if self.is_little_endian else '>H', data)[0]
    
    def read_uint32(self, f) -> int:
        data = f.read(4)
        if len(data) != 4:
            return 0
        return struct.unpack('<I' if self.is_little_endian else '>I', data)[0]
    
    def read_data_at_offset(self, f, offset: int, count: int) -> bytes:
        current_pos = f.tell()
        f.seek(offset)
        data = f.read(count)
        f.seek(current_pos)
        return data
    
    def scan_ifd_for_exif(self, f, offset: int) -> Optional[int]:
        """Scan IFD looking for EXIF sub-directory (tag 34665)"""
        if offset == 0:
            return None
            
        f.seek(offset)
        num_entries = self.read_uint16(f)
        
        if num_entries > 1000:  # Sanity check
            return None
            
        for _ in range(num_entries):
            tag = self.read_uint16(f)
            entry_type = self.read_uint16(f)
            count = self.read_uint32(f)
            value_offset = self.read_uint32(f)
            
            if tag == 34665:  # EXIF IFD tag
                return value_offset
                
        return None
    
    def scan_exif_for_origin_data(self, f, exif_offset: int) -> Optional[str]:
        """Scan EXIF IFD looking for Origin JSON data (tag 37500)"""
        f.seek(exif_offset)
        num_entries = self.read_uint16(f)
        
        if num_entries > 100:  # Sanity check for EXIF
            return None
            
        for _ in range(num_entries):
            tag = self.read_uint16(f)
            entry_type = self.read_uint16(f)
            count = self.read_uint32(f)
            value_offset = self.read_uint32(f)
            
            if tag == 37500 and entry_type == 7:  # Tag 37500, UNDEFINED type
                # Read the binary data
                data = self.read_data_at_offset(f, value_offset, count)
                
                # Convert to string, stopping at null terminators
                text = ""
                for byte in data:
                    if byte == 0:
                        break
                    if 32 <= byte <= 126:  # Printable ASCII
                        text += chr(byte)
                
                # Check if this looks like Origin JSON
                if "StackedInfo" in text and "{" in text:
                    return text
                    
        return None
    
    def extract_astronomical_metadata(self) -> Optional[Dict[str, Any]]:
        """Extract and parse Origin astronomical metadata from TIFF file"""
        try:
            with open(self.filename, 'rb') as f:
                # Read TIFF header
                byte_order = f.read(2)
                if byte_order == b'II':
                    self.is_little_endian = True
                elif byte_order == b'MM':
                    self.is_little_endian = False
                else:
                    return None
                
                magic = self.read_uint16(f)
                if magic != 42:
                    return None
                    
                first_ifd_offset = self.read_uint32(f)
                
                # Find EXIF IFD
                exif_offset = self.scan_ifd_for_exif(f, first_ifd_offset)
                if not exif_offset:
                    return None
                
                # Find Origin JSON data in EXIF
                json_text = self.scan_exif_for_origin_data(f, exif_offset)
                if not json_text:
                    return None
                
                # Parse JSON
                return json.loads(json_text)
                
        except (IOError, json.JSONDecodeError, struct.error):
            return None
    
    def display_astronomical_info(self):
        """Display astronomical metadata in a clean format"""
        metadata = self.extract_astronomical_metadata()
        if not metadata:
            print(f"No Origin astronomical metadata found in {self.filename}")
            return
        
        stacked_info = metadata.get("StackedInfo", {})
        
        print(f"Astronomical Image: {self.filename}")
        print("=" * 50)
        
        # Object information
        object_name = stacked_info.get("objectName", "Unknown")
        print(f"Target: {object_name}")
        
        # Observation details
        date_time = stacked_info.get("dateTime", "Unknown")
        if date_time != "Unknown":
            # Clean up the timestamp format
            date_time = date_time.replace("T", " ").replace("+0100", " (UTC+1)")
        print(f"Date/Time: {date_time}")
        
        # Location (rounded for privacy)
        gps = stacked_info.get("gps", {})
        if gps:
            lat = round(gps.get("latitude", 0), 1)
            lon = round(gps.get("longitude", 0), 1)
            alt = gps.get("altitude", 0)
            print(f"Location: {lat}°N, {lon}°E (altitude: {alt}m)")
        
        # Capture parameters
        capture_params = stacked_info.get("captureParams", {})
        if capture_params:
            exposure = capture_params.get("exposure", 0)
            iso = capture_params.get("iso", 0)
            temp = capture_params.get("temperature", 0)
            binning = capture_params.get("binning", 1)
            auto_exp = capture_params.get("autoExposure", False)
            
            print(f"\nCapture Settings:")
            print(f"  Exposure: {exposure}s")
            print(f"  ISO: {iso}")
            print(f"  Temperature: {temp}°C")
            print(f"  Binning: {binning}x{binning}")
            print(f"  Auto Exposure: {'Yes' if auto_exp else 'No'}")
        
        # Stacking information
        stacked_depth = stacked_info.get("stackedDepth", 0)
        total_duration_ms = stacked_info.get("totalDurationMs", 0)
        total_minutes = round(total_duration_ms / 60000, 1) if total_duration_ms else 0
        
        print(f"\nStacking Information:")
        print(f"  Frames stacked: {stacked_depth}")
        print(f"  Total integration time: {total_minutes} minutes")
        
        # Technical details
        filter_name = stacked_info.get("filter", "Unknown")
        bayer = stacked_info.get("bayer", "Unknown")
        
        print(f"\nTechnical Details:")
        print(f"  Filter: {filter_name}")
        print(f"  Bayer pattern: {bayer.upper()}")
        
        # Field of view
        fov_x = stacked_info.get("fovX", 0)
        fov_y = stacked_info.get("fovY", 0)
        if fov_x and fov_y:
            fov_x_deg = round(fov_x * 180 / 3.14159, 3)  # Convert radians to degrees
            fov_y_deg = round(fov_y * 180 / 3.14159, 3)
            print(f"  Field of view: {fov_x_deg}° x {fov_y_deg}°")
        
        # Processing settings
        stretch_bg = stacked_info.get("stretchBackground", 0)
        stretch_str = stacked_info.get("stretchStrength", 0)
        if stretch_bg or stretch_str:
            print(f"\nProcessing:")
            print(f"  Stretch background: {stretch_bg}")
            print(f"  Stretch strength: {stretch_str}")

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 tiff_astro_reader.py <tiff_file>")
        sys.exit(1)
    
    filename = sys.argv[1]
    reader = TIFFAstroReader(filename)
    reader.display_astronomical_info()

if __name__ == "__main__":
    main()
