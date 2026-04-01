#!/usr/bin/env python3
import os
import sys

method = os.environ.get("REQUEST_METHOD", "")
body = ""
if method == "POST":
    length = int(os.environ.get("CONTENT_LENGTH", "0") or "0")
    if length > 0:
        body = sys.stdin.read(length)

print("Content-Type: text/html")
print("")
print("<!DOCTYPE html>")
print("<html><head><title>POST test</title></head><body>")
print("<h1>POST test</h1>")
print("<p>Method: {}</p>".format(method))
if body:
    print("<p>Body received: {}</p>".format(body))
else:
    print("<p>No body received</p>")
print("""
<form method="POST" action="/cgi-bin/post.py">
  <input type="text" name="data" placeholder="Enter something">
  <input type="submit" value="Send">
</form>
""")
print("</body></html>")
