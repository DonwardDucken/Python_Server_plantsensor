from http.server import BaseHTTPRequestHandler, HTTPServer, SimpleHTTPRequestHandler
import json

class SimpleHandler(SimpleHTTPRequestHandler):
    def do_POST(self):
        content_length = int(self.headers['Content-Length'])
        raw_data = self.rfile.read(content_length)
        print("\n Raw Data: ", raw_data)
        try:
            data = json.loads(raw_data)  # JSON → Python dict
            mac = data['mac']
            print("JSON empfangen:")
            print(data)


            self.send_response(200)
            self.end_headers()
            self.wfile.write(b"JSON OK")

        except json.JSONDecodeError:
            self.send_response(400)
            self.end_headers()
            self.wfile.write(b"Invalid JSON")

server = HTTPServer(("0.0.0.0", 8080), SimpleHandler)
print("Server läuft...")
server.serve_forever()
