import http.server
import socketserver
import webbrowser
import os
import sys

PORT = 8000
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DIST_DIR = os.path.join(BASE_DIR, "dist")

# If production build (dist) exists, serve from dist
if os.path.exists(os.path.join(DIST_DIR, "index.html")):
    DIRECTORY = DIST_DIR
    INDEX_URL = f"http://localhost:{PORT}/"
else:
    DIRECTORY = BASE_DIR
    INDEX_URL = f"http://localhost:{PORT}/index.html"

class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=DIRECTORY, **kwargs)

def run():
    os.chdir(DIRECTORY)
    with socketserver.TCPServer(("", PORT), Handler) as httpd:
        print("=" * 60)
        print("  PUPPYBOT6 PRO MISSION CONTROL & SEQUENCE STUDIO")
        print(f"  Serving from: {DIRECTORY}")
        print(f"  URL: {INDEX_URL}")
        print("  Opening browser automatically...")
        print("  Press Ctrl+C to stop the server.")
        print("=" * 60)
        try:
            webbrowser.open(INDEX_URL)
        except Exception:
            pass
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nServer stopped.")

if __name__ == "__main__":
    run()
