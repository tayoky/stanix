#!/bin/python

import requests
import os

#start by generating the markdown version of this
self = open("update-packages.py","r")
md = open("update-packages.md","w")
md.write("```python\n")
for line in self.readlines():
    md.write(line)
md.write("```\n")

api = "https://api.github.com"
repo = "tayoky/ports"

url = f"{api}/repos/{repo}/contents/ports"

response = requests.get(url=url)
data = response.json()

os.system("rm -fr packages")
os.mkdir("packages")

pak = open("packages/index.md","w")
pak.write("---\n"
            "title: packages\n"
            "comment: this file was generated automaticly DO NOT EDIT\n"
            "---\n"
            "there are some packages and port for stanix, some comes with precompiled\n"
             "## list\n")

for port in data:
    name = port["name"]
    path = (f"packages/{name}")
    print(name)
    os.mkdir(path)

    #try to get the manifest
    req = requests.get(f"https://raw.githubusercontent.com/{repo}/main/ports/{name}/{name}.sh") 
    manifest = req.text

    index = open(f"{path}/index.md","w")
    index.write("---\n")
    index.write(f"title: {name}\n")
    index.write("comment: this file was generated automaticly DO NOT EDIT\n")
    index.write("---\n")
    index.write("## description\n")
    index.write("\n## build\n")
    index.write("to build and install this package use the ports submodule in the stanix repo\n")
    index.write("after having making stanix\n")
    index.write("```sh\n")
    index.write("cd ports\n")
    index.write(f"./clean.sh {name}\n")
    index.write(f"./build.sh {name}\n")
    index.write(f"./install.sh {name}\n")
    index.write("```\n")
    index.write("\n## precompiled\n")
    index.write("precompiled are currently not available\n")
    index.write("\n## packages source\n")
    index.write(f"[package source]({port['html_url']})  \n")
    index.write("\n### manifest\n")
    index.write(f"```ini\n{manifest}```\n")
    index.write("\nthis page was generated using a [script](../../update-packages.md)\n")
    index.close()

    pak.write(f"[{name}]({name})  \n")

pak.write("\nthis page was generated using a [script](../update-packages.md)\n")
