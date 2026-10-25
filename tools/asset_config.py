"""Private asset paths shared by the HD pack compilers."""
import os
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
ASSET_DIR = Path(os.environ.get('ZELDA_ASSET_DIR', REPO_ROOT / 'package/apps/zelda1hd'))
if not os.environ.get('ZELDA_HD_PACK'):
    raise SystemExit('Use prepare_assets.py, or set ZELDA_HD_PACK to your unpacked ZeldaRemastered folder.')
PACK_DIR = Path(os.environ['ZELDA_HD_PACK'])
REPORT_DIR = REPO_ROOT / 'build/asset-reports'
REPORT_DIR.mkdir(parents=True, exist_ok=True)
