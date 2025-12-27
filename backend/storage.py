"""
Supabase Storage operations for ScareCrowWeb.

Handles image upload, retrieval, and deletion using Supabase Storage.
"""
import os
from typing import Optional
from dotenv import load_dotenv
from supabase import create_client, Client

# Load environment variables
load_dotenv()

# Supabase client
SUPABASE_URL = os.environ.get('SUPABASE_URL')
SUPABASE_KEY = os.environ.get('SUPABASE_KEY')
STORAGE_BUCKET = os.environ.get('STORAGE_BUCKET', 'scarecrow-images')

if not SUPABASE_URL or not SUPABASE_KEY:
    raise ValueError("SUPABASE_URL and SUPABASE_KEY environment variables are required")

supabase: Client = create_client(SUPABASE_URL, SUPABASE_KEY)


def upload_image(file_data: bytes, filename: str) -> Optional[str]:
    """
    Upload image to Supabase Storage.
    
    Args:
        file_data: Raw image bytes
        filename: Filename to save as
        
    Returns:
        Public URL of the uploaded image, or None on error
    """
    try:
        # Upload to storage bucket
        result = supabase.storage.from_(STORAGE_BUCKET).upload(
            path=filename,
            file=file_data,
            file_options={"content-type": "image/jpeg"}
        )
        
        # Get public URL
        public_url = supabase.storage.from_(STORAGE_BUCKET).get_public_url(filename)
        print(f"✅ Image uploaded: {filename}")
        return public_url
        
    except Exception as e:
        print(f"❌ Error uploading image: {e}")
        return None


def get_image_url(filename: str) -> str:
    """
    Get public URL for an image.
    
    Args:
        filename: Name of the file in storage
        
    Returns:
        Public URL of the image
    """
    return supabase.storage.from_(STORAGE_BUCKET).get_public_url(filename)


def delete_image(filename: str) -> bool:
    """
    Delete an image from storage.
    
    Args:
        filename: Name of the file to delete
        
    Returns:
        True if deleted successfully, False otherwise
    """
    try:
        supabase.storage.from_(STORAGE_BUCKET).remove([filename])
        print(f"✅ Image deleted: {filename}")
        return True
    except Exception as e:
        print(f"❌ Error deleting image: {e}")
        return False


def init_storage():
    """
    Verify storage bucket exists and is accessible.
    """
    try:
        # Try to list files (will fail if bucket doesn't exist)
        supabase.storage.from_(STORAGE_BUCKET).list(limit=1)
        print(f"✅ Supabase Storage bucket '{STORAGE_BUCKET}' ready")
        return True
    except Exception as e:
        print(f"⚠️ Storage bucket warning: {e}")
        print(f"   Create bucket '{STORAGE_BUCKET}' in Supabase Dashboard")
        return False
