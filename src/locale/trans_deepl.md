# DeepL Translation Service Setup

Get your free API key from: https://www.deepl.com/pro-api
Free tier: 500,000 characters/month

## Setup Instructions

1. **Sign up** at https://www.deepl.com/pro-api (free tier available)
2. **Copy your API key**
3. **Create file** `trans_deepl.key` in this directory
4. **Paste your API key** into the file (one line, no quotes needed)

Example `trans_deepl.key` content:
```
012abc34-56d7-890e-f123-abc456d7e8f9:fx
```

**Note**: The `.key` file is gitignored and will not be committed.

## Installation (IMPORTANT - use official DeepL Python library)

```bash
pip uninstall deepl deepl-cli -y
pip install --upgrade deepl
```

## Testing Translation

Test the translation module directly. All text after the language codes is treated as input:

```bash
# Single word (no quotes needed)
py trans_deepl.py en_US de_DE Hello

# Multiple words (shell requires quotes to group them)
py trans_deepl.py en_US de_DE "Hello World"
py trans_deepl.py en_US hr_HR "Time Sync Interval (hours)"

# Text containing quotes (PowerShell - use single quotes outside)
py trans_deepl.py en_US es_ES '"He said Hello"'
```

**Note**: When called programmatically by www_tool.py or display_tool.py, all characters are passed exactly (no shell processing).

**Known Limitation**: DeepL API may normalize punctuation (quotes, apostrophes) as it's designed for natural language translation, not exact character preservation. Example:
- Input: `"Hello"` → Output: `Hallo` (quotes removed)
- Input: `It's great` → Output: `Es ist großartig` (apostrophe preserved in context)

For UI translations, this is rarely an issue as most strings don't contain literal quote marks.

## Check API Usage Quota

### PowerShell
Check if your key ends in `:fx` for Free API:
```powershell
# For Free API keys (ending in :fx):
Invoke-RestMethod -Uri "https://api-free.deepl.com/v2/usage" -Headers @{"Authorization"="DeepL-Auth-Key YOUR-API-KEY-HERE"} | Format-List

# For Pro API keys:
Invoke-RestMethod -Uri "https://api.deepl.com/v2/usage" -Headers @{"Authorization"="DeepL-Auth-Key YOUR-API-KEY-HERE"} | Format-List
```

### Git Bash / WSL / Linux
```bash
# For Free API (keys ending in :fx):
curl -H "Authorization: DeepL-Auth-Key YOUR-API-KEY-HERE" https://api-free.deepl.com/v2/usage

# For Pro API:
curl -H "Authorization: DeepL-Auth-Key YOUR-API-KEY-HERE" https://api.deepl.com/v2/usage
```

### Python
Automatically detects Free vs Pro:
```python
import deepl
translator = deepl.Translator("YOUR-API-KEY-HERE")
usage = translator.get_usage()
print(f"Characters: {usage.character.count} / {usage.character.limit}")
```
