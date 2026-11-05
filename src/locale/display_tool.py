#!/usr/bin/env python3
"""
Manage display locale JSON files in src/locale/display/ — compare against master JSON.

NOTE:
    Check .md files for how to install full translation support

USAGE:
    python display_tool.py <target> [mode] [options]
    python display_tool.py <master> <target> [mode] [options]
    python display_tool.py * [mode] [options]

TARGET:
    <target>         One locale → check against en_US.json (default master)
    <master> <target> Explicit master + target pair
    *                All locales → all .json files EXCEPT the master

MODES:
    (default)        Interactive mode - prompts for missing keys, ask about cleanup, ask about sort
    --fast, -f       Add all missing keys at once using master text, skip individual edits
    --every, -e      Prompt to review every single key using master text (detailed proofreading)
    --diff, -d       Only prompt when target text differs from master (to find changed translations)
    --ndiff, -n      Only prompt when target text is same as master (to fix untranslated text)

OPTIONS:
    --translate, -t  Translate master text (can't use with --diff)
    --clean, -c      Auto-delete extra keys not in master (no prompt; never touches master)
    --sort, -s       Auto-sort keys to match master's key order (no prompt)
    --create         Auto-create missing locale JSON from master with empty values

EXAMPLES:
    # Interactive check of one file (en_US is default master)
    py display_tool.py fr_FR

    # Explicit master + target with translation
    py display_tool.py en_US fr_FR --translate

    # Process all non-master files with translation, clean, and sort
    py display_tool.py * --translate --clean --sort

    # Fast mode WITH translation (auto-translate all missing keys in all files)
    py display_tool.py * --translate --fast --clean --sort

    # Diff mode (find keys where target differs from master)
    py display_tool.py fr_FR --diff
"""

import os
import sys
import json
import re
import argparse
import glob
import subprocess
import shlex
try:
    import msvcrt  # Windows
    WINDOWS = True
except ImportError:
    import termios
    import tty
    WINDOWS = False

# Translation service configuration (shared with www_tool.py)
_translation_service = None
_translation_check_done = False
_translation_input_locale = "en_US"

_translation_lang_cache = {}
_translation_error_shown = False

META_KEYS = ('locale_code', 'locale', 'locale_en')  # tuple — order matters for sort_keys_by_master

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DISPLAY_DIR = os.path.join(SCRIPT_DIR, 'display')
DEFAULT_MASTER = 'en_US'


def detect_translation_service():
    """
    Detect available translation service by scanning for trans_*.key files.
    Returns: service name (e.g., 'deepl', 'google') or None if none available
    """
    global _translation_service, _translation_check_done

    if _translation_check_done:
        return _translation_service

    _translation_check_done = True

    # Scan for any trans_*.key files
    key_files = glob.glob(os.path.join(SCRIPT_DIR, 'trans_*.key'))

    for key_file in sorted(key_files):
        basename = os.path.basename(key_file)
        service_name = basename[6:-4]  # Remove 'trans_' prefix and '.key' suffix

        has_key = False
        try:
            with open(key_file, 'r', encoding='utf-8') as f:
                for line in f:
                    line = line.strip()
                    if line and not line.startswith('#'):
                        has_key = True
                        break
        except Exception:
            continue

        if not has_key:
            continue

        script_file = os.path.join(SCRIPT_DIR, f'trans_{service_name}.py')
        if os.path.exists(script_file):
            _translation_service = service_name
            return _translation_service

    _translation_service = None
    return _translation_service


def translate_text(text, source_locale=None, target_locale=None):
    """
    Translate text using available translation service.
    Returns translated text or None if translation failed.
    """
    global _translation_lang_cache, _translation_input_locale, _translation_error_shown

    if source_locale is None:
        source_locale = _translation_input_locale

    service = detect_translation_service()
    if not service or not target_locale:
        return None

    if target_locale in _translation_lang_cache and not _translation_lang_cache[target_locale]:
        return None

    if service:
        script_path = os.path.join(SCRIPT_DIR, f'trans_{service}.py')

        try:
            result = subprocess.run(
                [sys.executable, script_path, source_locale, target_locale, text],
                capture_output=True,
                timeout=30,
                text=True,
                encoding='utf-8',
                errors='replace'
            )

            if result.returncode == 0:
                translated = result.stdout.strip()
                if translated:
                    _translation_lang_cache[target_locale] = True
                    return translated

            if result.stderr and not _translation_error_shown:
                error_msg = result.stderr.strip()
                if error_msg:
                    print(f"\n⚠ Translation error: {error_msg}")
                    print("  Source text requires confirmation per key.\n")
                    _translation_error_shown = True

            _translation_lang_cache[target_locale] = False
            return None

        except subprocess.TimeoutExpired:
            if not _translation_error_shown:
                print(f"\n⚠ Translation timeout (>30s) for {target_locale}")
                print("  Source text requires confirmation per key.\n")
                _translation_error_shown = True
            _translation_lang_cache[target_locale] = False
            return None
        except (UnicodeDecodeError, UnicodeError) as e:
            if not _translation_error_shown:
                print(f"\n⚠ Translation encoding error for {target_locale}: {e}")
                print("  Source text requires confirmation per key.\n")
                _translation_error_shown = True
            _translation_lang_cache[target_locale] = False
            return None
        except Exception as e:
            if not _translation_error_shown:
                print(f"\n⚠ Translation error: {e}")
                print("  Source text requires confirmation per key.\n")
                _translation_error_shown = True
            _translation_lang_cache[target_locale] = False
            return None

    return None


def get_key():
    """Get a single keypress (cross-platform)."""
    if WINDOWS:
        return msvcrt.getch()
    else:
        fd = sys.stdin.fileno()
        old_settings = termios.tcgetattr(fd)
        try:
            tty.setraw(sys.stdin.fileno())
            ch = sys.stdin.read(1)
        finally:
            termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
        return ch


def confirm_source_text_use(key, source_text, reason, keep_existing=False):
    """Ask before writing source text when translation is missing/unchanged."""
    print(f"\n⚠ {reason}: {key}")
    print(f"[Source] {source_text}")
    suffix = "(n keeps JSON)" if keep_existing else "(n skips key)"
    print(f"Use source text anyway? [y/n] {suffix}: ", end='', flush=True)
    return input().strip().lower() == 'y'


def prompt_for_key(key, found_text, json_text=None, filename=None, mode='missing', locale_code=None, use_translate=False):
    """Prompt user for translation text."""
    print()

    translated_text = None
    if use_translate and locale_code and locale_code != 'en_US':
        translated_text = translate_text(found_text, target_locale=locale_code)
        failed_or_same = (not translated_text) or translated_text == found_text
        if failed_or_same:
            reason = "Translation failed" if not translated_text else "Translation returned unchanged source text"
            keep_existing = mode != 'missing'
            if not confirm_source_text_use(key, found_text, reason, keep_existing=keep_existing):
                return json_text if keep_existing else None
            translated_text = None

    if mode == 'missing':
        print(f"[{filename}] {key}")
        print(f"[Found] {found_text}")

        if translated_text:
            print(f"[Translation] {translated_text}")
            default_text = translated_text
            prompt_msg = "Enter new text (ENTER accepts Translation / type to edit / ESC skip): "
        else:
            default_text = found_text
            prompt_msg = "Enter new text (ENTER accepts Found / type to edit / ESC skip): "

        print(prompt_msg, end='', flush=True)

        user_input = ""
        while True:
            if WINDOWS:
                ch = msvcrt.getch()
                if ch == b'\r':
                    result = user_input if user_input else default_text
                    if user_input:
                        print()
                    else:
                        source = "Translation" if translated_text else "Found"
                        print(f"[ENTER - using {source} text: {result}]")
                    return result
                elif ch == b'\x1b':
                    print("[ESC - skipping this key]")
                    return None
                elif ch == b'\x08':
                    if user_input:
                        user_input = user_input[:-1]
                        print('\b \b', end='', flush=True)
                elif ch in (b'\x03', b'\x04'):
                    print()
                    sys.exit(0)
                else:
                    try:
                        char = ch.decode('utf-8')
                        user_input += char
                        print(char, end='', flush=True)
                    except:
                        pass
            else:
                ch = get_key()
                if ch == '\r' or ch == '\n':
                    result = user_input if user_input else default_text
                    if user_input:
                        print()
                    else:
                        source = "Translation" if translated_text else "Found"
                        print(f"[ENTER - using {source} text: {result}]")
                    return result
                elif ch == '\x1b':
                    print("[ESC - skipping this key]")
                    return None
                elif ch == '\x7f':
                    if user_input:
                        user_input = user_input[:-1]
                        print('\b \b', end='', flush=True)
                elif ch in ('\x03', '\x04'):
                    print()
                    sys.exit(0)
                else:
                    user_input += ch
                    print(ch, end='', flush=True)

    else:  # 'all', 'diff', or 'ndiff' mode
        print(f"[{filename}] {key}")
        print(f"[Found] {found_text}")

        if translated_text:
            print(f"[Translation] {translated_text}")

        if json_text is not None:
            print(f"[JSON] {json_text}")

        if translated_text:
            default_text = translated_text
            prompt_msg = "Enter new text (ENTER accepts Translation / type to edit / ESC keeps JSON): "
        else:
            default_text = found_text
            prompt_msg = "Enter new text (ENTER accepts Found / type to edit / ESC keeps JSON): "

        print(prompt_msg, end='', flush=True)

        user_input = ""
        while True:
            if WINDOWS:
                ch = msvcrt.getch()
                if ch == b'\r':
                    result = user_input if user_input else default_text
                    if user_input:
                        print()
                    else:
                        source = "Translation" if translated_text else "Found"
                        print(f"[ENTER - using {source} text: {result}]")
                    return result
                elif ch == b'\x1b':
                    result = json_text if json_text is not None else default_text
                    print(f"[ESC - keeping JSON text: {result}]")
                    return result
                elif ch == b'\x08':
                    if user_input:
                        user_input = user_input[:-1]
                        print('\b \b', end='', flush=True)
                elif ch in (b'\x03', b'\x04'):
                    print()
                    sys.exit(0)
                else:
                    try:
                        char = ch.decode('utf-8')
                        user_input += char
                        print(char, end='', flush=True)
                    except:
                        pass
            else:
                ch = get_key()
                if ch == '\r' or ch == '\n':
                    result = user_input if user_input else default_text
                    if user_input:
                        print()
                    else:
                        source = "Translation" if translated_text else "Found"
                        print(f"[ENTER - using {source} text: {result}]")
                    return result
                elif ch == '\x1b':
                    result = json_text if json_text is not None else default_text
                    print(f"[ESC - keeping JSON text: {result}]")
                    return result
                elif ch == '\x7f':
                    if user_input:
                        user_input = user_input[:-1]
                        print('\b \b', end='', flush=True)
                elif ch in ('\x03', '\x04'):
                    print()
                    sys.exit(0)
                else:
                    user_input += ch
                    print(ch, end='', flush=True)


def sort_keys_by_master(master_data, target_data):
    """
    Sort target JSON keys to match master's key order.
    New keys in target that don't exist in master are appended at the end (alphabetically).
    Metadata keys (locale_code, locale, locale_en) always come first.
    """
    master_keys = list(master_data.keys())

    # Start with meta keys in order
    result = {}
    for mk in META_KEYS:
        if mk in target_data:
            result[mk] = target_data[mk]

    # Follow master's order for translation keys
    for key in master_keys:
        if key in target_data and key not in META_KEYS:
            result[key] = target_data[key]

    # Append any keys in target that don't exist in master (alphabetically)
    extra_keys = sorted(k for k in target_data if k not in result and k not in META_KEYS)
    for key in extra_keys:
        result[key] = target_data[key]

    return result


def create_locale_from_master(master_data, locale_code):
    """Create a new locale JSON from master with empty translation values."""
    new_data = {}
    for key, value in master_data.items():
        if key == 'locale_code':
            new_data[key] = locale_code
        elif key in ('locale', 'locale_en'):
            new_data[key] = ''
        else:
            new_data[key] = ''  # All translation values empty
    return new_data


def load_json_safe(path):
    """Load JSON with automatic trailing comma fix."""
    try:
        with open(path, 'r', encoding='utf-8') as f:
            return json.load(f)
    except json.JSONDecodeError as e:
        if 'trailing comma' in str(e).lower() or 'illegal trailing comma' in str(e).lower():
            print(f"⚠ Found trailing comma error in JSON file - attempting to fix...")
            with open(path, 'r', encoding='utf-8') as f:
                json_text = f.read()
            fixed_json = re.sub(r',(\s*[}\]])', r'\1', json_text)
            try:
                data = json.loads(fixed_json)
                with open(path, 'w', encoding='utf-8') as f:
                    json.dump(data, f, ensure_ascii=False, indent=2)
                print(f"✓ Automatically fixed and saved {os.path.basename(path)}")
                return data
            except json.JSONDecodeError as e2:
                print(f"\nError: Could not parse JSON file even after fixing trailing commas")
                print(f"  {e2}")
                return None
        else:
            print(f"\nError: Invalid JSON in {path}")
            print(f"  {e}")
            return None


def get_translation_keys(data):
    """Return list of translation keys (excludes meta keys), preserving order."""
    return [k for k in data if k not in META_KEYS]


def process_display_locale(locale_code, master_code, master_data, mode, auto_clean, auto_sort, use_translate=False, auto_create=False):
    """Process a single display locale file against the master."""
    print(f"\n{'='*60}")
    print(f"Processing: {locale_code}.json  (master: {master_code}.json)")
    print(f"{'='*60}")

    json_path = os.path.join(DISPLAY_DIR, f'{locale_code}.json')

    if not os.path.exists(json_path):
        if auto_create:
            new_data = create_locale_from_master(master_data, locale_code)
            with open(json_path, 'w', encoding='utf-8') as f:
                json.dump(new_data, f, ensure_ascii=False, indent=2)
            print(f"✓ Created {locale_code}.json from {master_code}.json with empty values")
        else:
            print(f"Error: JSON file not found at {json_path}")
            return False

    locale_data = load_json_safe(json_path)
    if locale_data is None:
        return False

    master_keys = get_translation_keys(master_data)
    target_keys = get_translation_keys(locale_data)

    print(f"Master {master_code}.json has {len(master_keys)} translation keys")
    print(f"Loaded {len(target_keys)} translation keys from {locale_code}.json")

    # Extract locale info for headers
    locale_native = locale_data.get('locale', '')
    locale_english = locale_data.get('locale_en', '')
    locale_display = f" ({locale_native} / {locale_english})" if locale_native and locale_english else ""

    # Show missing keys summary
    if mode in ('missing', 'fast'):
        missing_keys = [k for k in master_keys if locale_data.get(k) is None]
        if missing_keys:
            print("\n" + "=" * 60)
            print(f"Keys in master not found in {locale_code}{locale_display}:")
            print("=" * 60)
            for key in missing_keys:
                print(f"  {key} = {master_data.get(key, '')}")
            print(f"\nTotal: {len(missing_keys)} missing key(s)")
            print("=" * 60)

    # Process keys
    updates = {}
    processed_count = 0

    if mode == 'fast':
        missing_keys = [(k, master_data.get(k, '')) for k in master_keys if locale_data.get(k) is None]
        if missing_keys:
            pending_updates = {}

            if use_translate and locale_code != 'en_US':
                print(f"\nAuto-translating {len(missing_keys)} missing keys...")
                print("  (✓ = translated, → = source confirmed, - = source skipped)\n")

                for key, found_text in missing_keys:
                    translated_text = translate_text(found_text, target_locale=locale_code)

                    if translated_text and translated_text != found_text:
                        pending_updates[key] = translated_text
                        print(f"  ✓ {key}: {translated_text}")
                    else:
                        reason = "Translation failed" if not translated_text else "Translation returned unchanged source text"
                        if confirm_source_text_use(key, found_text, reason):
                            pending_updates[key] = found_text
                            print(f"  → {key}: {found_text}")
                        else:
                            print(f"  - {key}: skipped")
            else:
                for key, found_text in missing_keys:
                    pending_updates[key] = found_text

            updates.update(pending_updates)
            processed_count = len(pending_updates)
            print(f"\n✓ Added {processed_count} key(s) to JSON")

    elif mode in ('missing', 'every', 'diff', 'ndiff'):
        for key in master_keys:
            found_text = master_data.get(key, '')
            json_text = locale_data.get(key)

            if mode == 'missing':
                if json_text is not None:
                    continue
                new_text = prompt_for_key(key, found_text, json_text, locale_code, mode='missing',
                                          locale_code=locale_code, use_translate=use_translate)
                if new_text is not None:
                    updates[key] = new_text
                    processed_count += 1

            elif mode == 'every':
                new_text = prompt_for_key(key, found_text, json_text, locale_code, mode='all',
                                          locale_code=locale_code, use_translate=use_translate)
                if new_text != json_text:
                    updates[key] = new_text
                    processed_count += 1

            elif mode == 'diff':
                if json_text is None or (found_text and json_text != found_text):
                    new_text = prompt_for_key(key, found_text, json_text, locale_code, mode='diff',
                                              locale_code=locale_code, use_translate=False)
                    if new_text != json_text:
                        updates[key] = new_text
                        processed_count += 1

            elif mode == 'ndiff':
                if json_text is not None and found_text and json_text == found_text:
                    new_text = prompt_for_key(key, found_text, json_text, locale_code, mode='ndiff',
                                              locale_code=locale_code, use_translate=use_translate)
                    if new_text != json_text:
                        updates[key] = new_text
                        processed_count += 1

    if updates:
        print(f"\n{len(updates)} key(s) updated")
        locale_data.update(updates)
    else:
        print("\nNo changes made")

    # Handle extra keys (keys in target not in master)
    extra = []
    for key in sorted(locale_data.keys()):
        if key not in master_data and key not in META_KEYS:
            extra.append(key)

    print("\n" + "=" * 60)
    print(f"Keys in {locale_code}.json not found in master {locale_display}:")
    print("=" * 60)

    if extra:
        for key in extra:
            print(f"  {key} = {locale_data[key]}")
        print(f"\nTotal: {len(extra)} extra key(s)")

        if auto_clean:
            for key in extra:
                del locale_data[key]
            print(f"✓ Auto-deleted {len(extra)} extra key(s)")
        else:
            print("\nWould you like to delete these keys from the JSON? [y/n]: ", end='', flush=True)
            response = input().strip().lower()
            if response == 'y':
                for key in extra:
                    del locale_data[key]
                print(f"✓ Deleted {len(extra)} extra key(s)")
            else:
                print("No keys deleted")
    else:
        print("  (none)")

    # Sort JSON (master key order)
    if auto_sort:
        locale_data = sort_keys_by_master(master_data, locale_data)
        print("\n✓ Auto-sorted keys to match master order")
    elif (updates or extra) and mode != 'fast':
        print("\nWould you like to sort keys to match master order? [y/n]: ", end='', flush=True)
        response = input().strip().lower()
        if response == 'y':
            locale_data = sort_keys_by_master(master_data, locale_data)
            print("✓ Sorted keys to match master order")

    # Write updated JSON
    temp_path = json_path + '.tmp'
    with open(temp_path, 'w', encoding='utf-8') as f:
        json.dump(locale_data, f, ensure_ascii=False, indent=2)

    os.replace(temp_path, json_path)
    print(f"\n✓ Saved {json_path}")

    return True


def main():
    # Reconfigure stdout for Windows cp949 terminal
    if sys.platform == 'win32':
        import io
        sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
        sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

    if len(sys.argv) == 1:
        print(__doc__)
        sys.exit(0)

    parser = argparse.ArgumentParser(
        description='Manage display locale JSON files against master JSON',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument('target', help='Locale code (e.g., fr_FR), or * for all non-master locales, or master code when two positional args')
    parser.add_argument('target2', nargs='?', default=None, help='Target locale code when first arg is the master')
    parser.add_argument('--fast', '-f', action='store_true', help='Add all missing keys at once (one prompt)')
    parser.add_argument('--translate', '-t', action='store_true', help='Use translation service for missing/new keys')
    parser.add_argument('--every', '-e', action='store_true', help='Prompt to review every single key')
    parser.add_argument('--diff', '-d', action='store_true', help='Only prompt when text differs')
    parser.add_argument('--ndiff', '-n', action='store_true', help='Only prompt when text is same (to fix untranslated)')
    parser.add_argument('--clean', '-c', action='store_true', help='Auto-delete extra keys not in master (no prompt; never touches master)')
    parser.add_argument('--sort', '-s', action='store_true', help='Auto-sort keys to match master order (no prompt)')
    parser.add_argument('--create', action='store_true', help='Auto-create missing locale JSON from master with empty values')
    args = parser.parse_args()

    # Determine master and target(s)
    if args.target2 is not None:
        # Two positional args: first is master, second is target
        master_code = args.target
        target_code = args.target2
        targets = [target_code]
    elif args.target == '*':
        # Wildcard: all files except the default master
        master_code = DEFAULT_MASTER
        targets = []
        for f in sorted(glob.glob(os.path.join(DISPLAY_DIR, '*.json'))):
            code = os.path.splitext(os.path.basename(f))[0]
            if code != master_code:
                targets.append(code)
        if not targets:
            print(f"Error: No locale files found in {DISPLAY_DIR} (excluding {master_code})")
            sys.exit(1)
    else:
        # Single arg: it's the target, master defaults to en_US
        master_code = DEFAULT_MASTER
        target_code = args.target
        targets = [target_code]

    # Validate argument combinations
    mode_count = sum([args.fast, args.every, args.diff, args.ndiff])
    if mode_count > 1:
        print("Error: Only one mode can be specified (--fast, --every, --diff, --ndiff)")
        sys.exit(1)

    if args.translate and args.diff:
        print("Error: --translate cannot be used with --diff mode")
        sys.exit(1)

    if args.target == '*' and (args.every or args.diff or args.ndiff):
        print("Error: * (all locales) cannot be combined with --every, --diff, or --ndiff")
        sys.exit(1)

    # Determine mode
    if args.fast:
        mode = 'fast'
    elif args.every:
        mode = 'every'
    elif args.diff:
        mode = 'diff'
    elif args.ndiff:
        mode = 'ndiff'
    else:
        mode = 'missing'

    print()

    # Check translation service availability
    service = detect_translation_service()

    if service:
        if not args.translate:
            if mode == 'missing':
                print(f"{'='*60}")
                print(f"✓ Translation service available: {service.upper()}")
                print(f"{'='*60}")
                print("Use translation service for missing/new keys? [y/n]: ", end='', flush=True)
                response = input().strip().lower()
                if response == 'y':
                    args.translate = True
                    print("✓ Translation enabled for this session\n")
                else:
                    print("→ Translation disabled, will use master text\n")
        else:
            print(f"{'='*60}")
            print(f"✓ Translation service: {service.upper()}")
            print(f"{'='*60}\n")
    else:
        if args.translate:
            print(f"{'='*60}")
            print("⚠ Error: --translate flag is set but translation is unavailable!")
            print("⚠ No translation service (add API key to trans_<service>.key)")
            print(f"{'='*60}")
            sys.exit(1)
        elif mode == 'missing':
            print(f"{'='*60}")
            print("⚠ Warning: No translation service found.")
            print("⚠ This tool is more powerful with a translation service!")
            print(f"{'='*60}\n")

    # Validate master exists
    master_path = os.path.join(DISPLAY_DIR, f'{master_code}.json')
    if not os.path.exists(master_path):
        print(f"Error: Master locale file not found at {master_path}")
        sys.exit(1)

    master_data = load_json_safe(master_path)
    if master_data is None:
        sys.exit(1)

    print(f"Master: {master_code}.json ({len(get_translation_keys(master_data))} keys)")

    if args.target == '*':
        print(f"Found {len(targets)} locale file(s) to process (excluding master: {master_code})")

    # Process all targets
    success_count = 0
    for tcode in targets:
        # Never allow clean/sort on master when using wildcard
        use_clean = args.clean and tcode != master_code
        use_sort = args.sort
        if process_display_locale(tcode, master_code, master_data, mode, use_clean, use_sort, args.translate, args.create):
            success_count += 1

    if len(targets) > 1:
        print(f"\n{'='*60}")
        print(f"Processed {success_count}/{len(targets)} locale file(s) successfully")
        print(f"{'='*60}")


if __name__ == '__main__':
    main()
