#!/usr/bin/env python3
import os
import sys

name = os.environ.get("QUERY_STRING", "")
if name.startswith("name="):
    name = name[5:]
else:
    name = "CGI"

print("Content-Type: text/html")
print("")
print("<!DOCTYPE html>")
print("<html><head><title>Hello</title></head><body>")
print("<h1>Hello, {}!</h1>".format(name))
print("<p>Method: {}</p>".format(os.environ.get("REQUEST_METHOD", "unknown")))
print("<p>Script: {}</p>".format(os.environ.get("SCRIPT_FILENAME", "unknown")))
print("</body></html>")
