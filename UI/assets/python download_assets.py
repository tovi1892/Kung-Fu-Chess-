import os
import urllib.request
import ssl

# הגדרת פייתון להתעלם מאימות תעודת ה-SSL של הסינון (נטפרי)
ssl._create_default_https_context = ssl._create_unverified_context

# יצירת תיקיית assets אם היא לא קיימת
assets_dir = "assets"
os.makedirs(assets_dir, exist_ok=True)

# רשימת הכלים שאנו צריכים
pieces = [
    'wK', 'wQ', 'wR', 'wB', 'wN', 'wP',
    'bK', 'bQ', 'bR', 'bB', 'bN', 'bP'
]

# כתובת ה-CDN של jsDelivr המארח את תמונות הכלים של פרויקט השחמט chessboardjs
base_url = "https://cdn.jsdelivr.net/gh/oakmac/chessboardjs/website/img/chesspieces/wikipedia/"

headers = {
    'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36'
}

print("Starting to download chess piece assets from jsDelivr CDN...")

for name in pieces:
    url = f"{base_url}{name}.png"
    target_path = os.path.join(assets_dir, f"{name}.png")
    try:
        print(f"Downloading {name}.png...")
        req = urllib.request.Request(url, headers=headers)
        with urllib.request.urlopen(req) as response:
            with open(target_path, 'wb') as f:
                f.write(response.read())
    except Exception as e:
        print(f"Failed to download {name}.png: {e}")

print("\nAll assets downloaded successfully! Check your 'assets' folder.")