#!/usr/bin/env python3
import os

print("Content-Type: text/html")
print("")
print("<!DOCTYPE html>")
print("<html><head><title>CGI Environment</title></head><body>")
print("<h1>CGI Environment Variables</h1>")
print("<table border='1'>")
for key, value in sorted(os.environ.items()):
    print("<tr><td>{}</td><td>{}</td></tr>".format(key, value))
print("</table>")
print("</body></html>")
