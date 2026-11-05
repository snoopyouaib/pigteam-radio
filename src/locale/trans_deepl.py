#!/usr/bin/env python3
"""
DeepL Translation Module
Usage: python trans_deepl.py source_lang target_lang "text to translate"
Example: python trans_deepl.py en_US de_DE "Hello"
Outputs: Translated text (single line)
Exit codes: 0 = success, 1 = error
"""

import sys
import os
import time

# Rate limiting and back-off configuration
# Initial delay before each API call (will be doubled on 429 errors)
initial_delay = 0.05  # 50ms - used for exponential backoff on 429 rate limit errors
max_delay = 2.0       # 2s maximum - if we exceed this, treat API as unavailable
max_retries = 5       # Maximum retry attempts on 429 errors

# DeepL-specific language code exceptions FOR TARGET languages
# Most languages: use first 2 letters (de_DE -> de)
# These need special handling:
DEEPL_TARGET_EXCEPTIONS = {
    'en_US': 'en-us',   # English US
    'en_GB': 'en-gb',   # English British  
    'no_NO': 'nb',      # Norwegian -> Bokmål
    'pt_BR': 'pt-br',   # Portuguese Brazilian
    'pt_PT': 'pt-pt',   # Portuguese European
    'zh_CN': 'zh',      # Chinese Simplified
    'zh_TW': 'zh',      # Chinese Traditional
}

def convert_to_deepl_code(locale_code, is_target=True):
    """
    Convert locale code (e.g., de_DE) to DeepL code (e.g., de).
    
    Args:
        locale_code: Locale code like 'de_DE', 'en_US', etc.
        is_target: True for target language, False for source language
    
    For source languages, DeepL uses simple 2-letter codes (EN, DE, FR).
    For target languages, some need special codes (EN-US, PT-BR, etc.).
    """
    if is_target and locale_code in DEEPL_TARGET_EXCEPTIONS:
        return DEEPL_TARGET_EXCEPTIONS[locale_code]
    else:
        # For source OR unlisted targets: use first 2 letters lowercase
        return locale_code[:2].lower()

def get_api_key():
    """Read API key from trans_deepl.key file."""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    key_file = os.path.join(script_dir, 'trans_deepl.key')
    
    if not os.path.exists(key_file):
        print("Error: trans_deepl.key not found", file=sys.stderr)
        return None
    
    try:
        with open(key_file, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                # Skip empty lines and comments
                if line and not line.startswith('#'):
                    return line
        print("Error: No API key found in trans_deepl.key", file=sys.stderr)
        return None
    except Exception as e:
        print(f"Error reading API key: {e}", file=sys.stderr)
        return None

def translate(text, source_lang, target_lang, api_key):
    """Translate text using DeepL API with exponential backoff on rate limits."""
    try:
        import deepl
    except ImportError:
        print("Error: deepl library not installed.", file=sys.stderr)
        print("Install with: pip install --upgrade deepl", file=sys.stderr)
        return None
    
    # Verify we have the correct deepl package
    if not hasattr(deepl, 'Translator'):
        print("Error: Wrong 'deepl' package installed.", file=sys.stderr)
        print("Uninstall any existing deepl packages:", file=sys.stderr)
        print("  pip uninstall deepl deepl-cli", file=sys.stderr)
        print("Then install the official DeepL Python library:", file=sys.stderr)
        print("  pip install --upgrade deepl", file=sys.stderr)
        return None
    
    # Convert locale codes to DeepL codes
    source_deepl = convert_to_deepl_code(source_lang, is_target=False)  # Source: simple 2-letter
    target_deepl = convert_to_deepl_code(target_lang, is_target=True)   # Target: may need exceptions
    
    # Exponential backoff retry loop
    current_delay = initial_delay
    retry_count = 0
    
    while retry_count <= max_retries:
        try:
            # Apply delay before API call (even on first attempt for rate limiting)
            time.sleep(current_delay)
            
            translator = deepl.Translator(api_key)
            result = translator.translate_text(text, source_lang=source_deepl.upper(), target_lang=target_deepl.upper())
            return result.text
            
        except deepl.DeepLException as e:
            # Check if it's a 429 rate limit error
            error_msg = str(e).lower()
            
            if hasattr(e, 'http_status_code') and e.http_status_code == 429:
                # Rate limit hit - apply exponential backoff
                retry_count += 1
                
                if current_delay * 2 > max_delay:
                    print(f"Error: DeepL rate limit exceeded, delays exceeded {max_delay}s threshold", file=sys.stderr)
                    return None
                
                # Double the delay for next attempt
                current_delay *= 2
                print(f"Rate limit (429) - retrying with {current_delay}s delay (attempt {retry_count}/{max_retries})...", file=sys.stderr)
                continue
                
            # Handle other DeepL errors
            if 'authorization' in error_msg or 'auth' in error_msg:
                print("Error: Invalid DeepL API key", file=sys.stderr)
            elif 'quota' in error_msg:
                print("Error: DeepL API quota exceeded", file=sys.stderr)
            else:
                print(f"Error translating: {e}", file=sys.stderr)
            return None
            
        except Exception as e:
            # Handle non-DeepL exceptions
            error_msg = str(e).lower()
            if 'authorization' in error_msg or 'auth' in error_msg:
                print("Error: Invalid DeepL API key", file=sys.stderr)
            elif 'quota' in error_msg:
                print("Error: DeepL API quota exceeded", file=sys.stderr)
            else:
                print(f"Error translating: {e}", file=sys.stderr)
            return None
    
    # Max retries exceeded
    print(f"Error: Max retry attempts ({max_retries}) exceeded for rate limiting", file=sys.stderr)
    return None

def main():
    # Reconfigure stdout/stderr to use UTF-8 encoding (for international characters)
    if sys.stdout.encoding != 'utf-8':
        import io
        sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
        sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')
    
    if len(sys.argv) < 4:
        print("Usage: python trans_deepl.py source_lang target_lang text...", file=sys.stderr)
        print("  All text after target_lang is treated as input (joined with spaces)", file=sys.stderr)
        print("Example: python trans_deepl.py en_US de_DE \"Hello World\"", file=sys.stderr)
        sys.exit(1)
    
    source_lang = sys.argv[1]
    target_lang = sys.argv[2]
    # Join all remaining arguments - preserves all characters when called programmatically
    text = ' '.join(sys.argv[3:])
    
    # Get API key
    api_key = get_api_key()
    if not api_key:
        sys.exit(1)
    
    # Translate
    translated = translate(text, source_lang, target_lang, api_key)
    if translated:
        print(translated)
        sys.exit(0)
    else:
        sys.exit(1)

if __name__ == "__main__":
    main()
