#
# Supermodel
# A Sega Model 3 Arcade Emulator.
# Copyright 2003-2023 The Supermodel Team
#
# This file is part of Supermodel.
#
# Supermodel is free software: you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free
# Software Foundation, either version 3 of the License, or (at your option)
# any later version.
#
# Supermodel is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License along
# with Supermodel.  If not, see <http://www.gnu.org/licenses/>.
#

#
# supermodel_build_bot.py
#
# Git snapshot build script. Checks latest Git commit against Supermodel3.com
# download page and builds and uploads a snapshot if needed. Intended to be run
# at least once daily as part of an automated job.
#
# Dependencies:
#   - MSYS2 installed at c:\msys64
#   - mingw64/mingw-w64-x86_64-gcc package
#   - mingw64/mingw-w64-x86_64-make package
#   - mingw64/mingw-w64-x86_64-SDL2 package
#   - mingw64/mingw-w64-x86_64-SDL2_net package
#   - msys/subversion package
#   - msys/zip package
#   - git
#   - paramiko (Python package for SSH/SFTP)
#
# To perform a test run:
#   - Download https://supermodel3.com/Download.html to the directory from
#     which the script will be run.
#   - Run: python update_model3emu_snapshot.py --working-dir=c:\tmp\build --test-run
#

import argparse
import base64
import os
import shutil
import sys
import tempfile

import paramiko

class CheckoutError(Exception):
  pass

class BuildError(Exception):
  pass

class PackageError(Exception):
  pass

class ParseError(Exception):
  pass

class Bash:
  def __init__(self, bash_path):
    self.bash_path = bash_path

  def execute(self, working_dir, command, log=False, print_output=False):
    import subprocess
    working_dir = working_dir.replace("\\", "/")  # convert to UNIX-style path
    invocation = self.bash_path + " -l -c '" + "cd " + working_dir + " && " + command + "'"
    if log:
      print("Executing: %s" % invocation)
    result = subprocess.run(invocation, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if print_output:
      print(result.stdout.decode())
    return result

def get_web_page(test_run):
  if test_run:
    with open("Download.html") as fp:
      html = fp.read()
  else:
    import urllib.request
    with urllib.request.urlopen("http://supermodel3.com/Download.html") as data:
      html = data.read().decode("utf-8")
  return html

def get_comment(line):
  comment_begin = line.find("<!--")
  comment_end = line.find("-->")
  if comment_begin >= 0 and comment_end > 0 and comment_end > comment_begin:
    return line[comment_begin+4:comment_end].strip()
  return None

def add_new_file(html, filename, version, git_sha, date):
  # Scan for: <!-- BEGIN_GIT_SNAPSHOTS $TEMPLATE -->
  lines = html.splitlines()
  for i in range(len(lines)):
    line = lines[i]
    text = get_comment(line)
    if text and text.startswith("BEGIN_GIT_SNAPSHOTS "):
      template = text[len("BEGIN_GIT_SNAPSHOTS "):]
      new_row = template.replace("$FILENAME", filename).replace("$VERSION", version).replace("$GIT_SHA", git_sha).replace("$DATE", date)
      git_tag = "<!-- GIT_SHA=%s -->" % git_sha
      lines.insert(i + 1, new_row + " " + git_tag)
      html = "\n".join(lines)
      return html
  raise ParseError("BEGIN_GIT_SNAPSHOTS not found")

def get_uploaded_git_shas(html):
  # Scan for all lines with: <!-- GIT_SHA=$SHA -->
  shas = []
  for line in html.splitlines():
    text = get_comment(line)
    if text and text.startswith("GIT_SHA"):
      tokens = text.split("=")
      if len(tokens) == 2:
        sha = tokens[1]
        shas.append(sha)
      else:
        raise ParseError("Error parsing GIT_SHA")
  shas.reverse()
  return shas

def create_change_log(bash, repo_dir, file_path, uploaded_shas, current_sha):
  # Log from first commit after 0.2a release until now
  result = bash.execute(working_dir=repo_dir, command="git -c color.ui=never log 06594a5..." + current_sha + " --no-decorate --pretty=\"Commit: %H%nAuthor: %an%nDate:   %ad%n%n%w(80,4,4)%B\"")
  if result.returncode != 0:
    return PackageError("Unable to obtain Git log")
  change_log = result.stdout.decode().strip()
  with open(file_path, "w", encoding="utf-8") as fp:
    header = "\n\n" \
      "  ####                                                      ###           ###\n" \
      " ##  ##                                                      ##            ##\n" \
      " ###     ##  ##  ## ###   ####   ## ###  ##  ##   ####       ##   ####     ##\n" \
      "  ###    ##  ##   ##  ## ##  ##   ### ## ####### ##  ##   #####  ##  ##    ##\n" \
      "    ###  ##  ##   ##  ## ######   ##  ## ####### ##  ##  ##  ##  ######    ##\n" \
      " ##  ##  ##  ##   #####  ##       ##     ## # ## ##  ##  ##  ##  ##        ##\n" \
      "  ####    ### ##  ##      ####   ####    ##   ##  ####    ### ##  ####    ####\n" \
      "                 ####\n" \
      "\n" \
      "                       A Sega Model 3 Arcade Emulator.\n" \
      "\n" \
      "                   Copyright 2003-2023 The Supermodel Team\n" \
      "\n" \
      "                                 CHANGE LOG\n" \
      "\n" \
      "\n"
    fp.write(header)
    fp.write(change_log)

def write_html_file(html, file_path):
  with open(file_path, "w") as fp:
    fp.write(html)

def upload(html_file, zip_file_path, username, password):
  # supermodel3.com host public key
  keydata=b"""AAAAE2VjZHNhLXNoYTItbmlzdHAyNTYAAAAIbmlzdHAyNTYAAABBBE49lZKcEsFhEfEgVc4iNrBKOtItoXqQ/TKkPH9bAWOfn25H9BAi5AjkpqSsv/p1T5qfDni5G9sajqzamHw0TmU="""
  key = paramiko.ECDSAKey(data=base64.decodebytes(keydata))

  # Create SFTP client
  ssh = paramiko.SSHClient()
  ssh.get_host_keys().add('supermodel3.com', 'ecdsa-sha2-nistp256', key)
  ssh.connect(hostname = "supermodel3.com", username = options.username, password = options.password)
  sftp = ssh.open_sftp()

  # Upload
  sftp.put(localpath = zip_file_path, remotepath = f"public_html/Files/Git_Snapshots/{os.path.basename(zip_file_path)}")
  sftp.put(localpath = html_file, remotepath = "public_html/Download.html")
  sftp.close()
  ssh.close()

def confirm_package_contents(package_dir, package_files):
  all_found = True
  for file in package_files:
    path = os.path.join(package_dir, file)
    if not os.path.exists(path):
      print("Missing package file: %s" % path)
      all_found = False
  if not all_found:
    raise PackageError("Failed to generate package files")

def rmdir(dir):
  def delete_readonly(action, name, exc):
    # https://stackoverflow.com/questions/2656322/shutil-rmtree-fails-on-windows-with-access-is-denied
    import stat
    if not os.access(name, os.W_OK):
      os.chmod(name, stat.S_IWUSR)
      action(name)
  for root, dirs, files in os.walk(dir):  # we expect .git to be write-protected
    if '.git' in dirs:
      shutil.rmtree(root+'\.git',onerror=delete_readonly)
  shutil.rmtree(dir)

def update_git_snapshot(working_dir, username, password, test_run, make):
  failed = False
  print("Starting %s in working directory: %s" % ("test run" if test_run else "release process", working_dir))

  try:
    bash = Bash(bash_path="c:\\msys64\\usr\\bin\\bash.exe")
    repo_dir = os.path.join(working_dir, "model3emu")

    # Clone Git repo. Unfortunately need to always do this in order to get short SHA
    result = bash.execute(working_dir=working_dir, command="git clone https://github.com/trzy/Supermodel.git model3emu")
    if result.returncode != 0:
      raise CheckoutError("Git clone failed")

    # Fetch Supermodel download page and compare most recent uploaded Git snapshot against current Git sha
    print("Fetching Supermodel download page...")
    html = get_web_page(test_run=test_run)
    uploaded_shas = get_uploaded_git_shas(html=html)
    last_uploaded_sha = uploaded_shas[-1] if len(uploaded_shas) > 0 else None
    print("Checking current Git SHA...")
    result = bash.execute(working_dir=repo_dir, command="git rev-parse --short HEAD")
    if result.returncode != 0:
      raise CheckoutError("Git HEAD SHA check failed")
    current_sha = result.stdout.decode().strip()

    # Get date of commit
    result = bash.execute(working_dir=repo_dir, command="git show -s --format=%as HEAD")
    if result.returncode != 0:
      raise CheckoutError("Unable to obtain HEAD commit date")
    current_date = result.stdout.decode().strip()

    # If Git has a newer version, or if performing a test run, build it and update the web page
    if current_sha == last_uploaded_sha and (not test_run):
      print("Nothing to do. Current Git SHA already uploaded: %s" % current_sha)
    else:
      # Check out
      print("Git SHA is %s but last uploaded SHA is %s" % (current_sha, last_uploaded_sha))

      # Build
      print("Building Supermodel...")
      result = bash.execute(working_dir=repo_dir, command=make + " -f Makefiles/Makefile.Win32 version")
      if result.returncode != 0:
        raise BuildError("Failed to obtain version")
      version = result.stdout.decode().strip()
      result = bash.execute(working_dir=repo_dir, command=make + " -f Makefiles/Makefile.Win32 release NET_BOARD=1", print_output=False)
      if result.returncode != 0:
        raise BuildError("Build failed")

      # Stage the release package files
      print("Creating release package...")
      pkg_dir = os.path.join(working_dir, "pkg")
      bash.execute(working_dir=working_dir, command="mkdir pkg && mkdir pkg/Config && mkdir pkg/NVRAM && mkdir pkg/Saves && mkdir pkg/ROMs && mkdir pkg/Assets")
      change_log_file_path = os.path.join(pkg_dir, "CHANGES.txt")
      create_change_log(bash, repo_dir=repo_dir, file_path=change_log_file_path, uploaded_shas=uploaded_shas, current_sha=current_sha)
      bash.execute(working_dir=working_dir, command="cp model3emu/Config/Supermodel.ini pkg/Config && cp model3emu/Config/Games.xml pkg/Config")
      bash.execute(working_dir=working_dir, command="echo NVRAM files go here. >pkg/NVRAM/DIR.txt")
      bash.execute(working_dir=working_dir, command="echo Save states go here. >pkg/Saves/DIR.txt")
      bash.execute(working_dir=working_dir, command="echo Recommended \\(but not mandatory\\) location for ROM sets. >pkg/ROMs/DIR.txt")
      bash.execute(working_dir=working_dir, command="cp model3emu/Assets/DIR.txt pkg/Assets && cp model3emu/Assets/p1crosshair.bmp pkg/Assets && cp model3emu/Assets/p2crosshair.bmp pkg/Assets")
      bash.execute(working_dir=working_dir, command="cp model3emu/Docs/README.txt pkg && cp model3emu/Docs/LICENSE.txt pkg")
      bash.execute(working_dir=working_dir, command="cp model3emu/bin64/supermodel.exe pkg/Supermodel.exe")
      #bash.execute(working_dir=working_dir, command="cp /mingw64/bin/SDL2.dll pkg && cp /mingw64/bin/SDL2_net.dll pkg")
      package_files = [
        "Supermodel.exe",
        #"SDL2.dll",
        #"SDL2_net.dll",
        "README.txt",
        "LICENSE.txt",
        "CHANGES.txt",
        "Config/Supermodel.ini",
        "Config/Games.xml",
        "NVRAM/DIR.txt",
        "Saves/DIR.txt",
        "ROMs/DIR.txt",
        "Assets/DIR.txt",
        "Assets/p1crosshair.bmp",
        "Assets/p2crosshair.bmp"
      ]
      confirm_package_contents(package_dir=pkg_dir, package_files=package_files)

      # Zip them up
      print("Compressing...")
      zip_file = "Supermodel_" + version + "_Win64.zip"
      zip_path = os.path.join(pkg_dir, zip_file)
      result = bash.execute(working_dir=pkg_dir, command="zip " + zip_file + " " + " ".join(package_files))
      if result.returncode != 0:
        raise PackageError("Failed to compress package")

      # Update the web page
      print("Updating Download page HTML...")
      html = add_new_file(html=html, filename=zip_file, version=version, git_sha=current_sha, date=current_date)
      html_file_path = os.path.join(working_dir, "Download.updated.html")
      write_html_file(html=html, file_path=html_file_path)
      if test_run:
        input("Test run finished. Press Enter to complete clean-up process...")
      else:
        print("Uploading Download page...")
        upload(html_file=html_file_path, zip_file_path=zip_path, username=username, password=password)

  except Exception as e:
    print("Error: %s" % str(e))
    failed = True
  except:
    print("Unknown error: ", sys.exc_info()[0])
    failed = True

  print("Cleaning up...")
  rmdir(working_dir)
  return failed

if __name__ == "__main__":
  parser = argparse.ArgumentParser()
  parser.add_argument("--username", metavar="user", type=str, action="store", help="Supermodel3.com FTP username")
  parser.add_argument("--password", metavar="pass", type=str, action="store", help="Supermodel3.com FTP password")
  parser.add_argument("--working-dir", metavar="path", type=str, action="store", help="Working directory to use (must not already exist); temporary directory if none specified")
  parser.add_argument("--test-run", action="store_true", help="Force a build without uploading and insert a pause")
  parser.add_argument("--make", metavar="command", type=str, default="mingw32-make", action="store", help="Make command to use")
  options = parser.parse_args()

  max_attempts = 3
  attempt = 1

  while attempt <= max_attempts:
    if options.working_dir:
      if os.path.exists(options.working_dir):
        raise Exception("Specified working directory (%s) already exists. This script requires a non-existent path that can safely be overwritten and then deleted." % options.working_dir)
      os.makedirs(options.working_dir)
      failed = update_git_snapshot(working_dir=options.working_dir, username=options.username, password=options.password, test_run=options.test_run, make=options.make)
    else:
      with tempfile.TemporaryDirectory() as working_dir:
        failed = update_git_snapshot(working_dir=working_dir, username=options.username, password=options.password, test_run=options.test_run, make=options.make)
    # Retry until success
    if not failed:
      break
    attempt += 1
    if attempt <= max_attempts:
      print("Release failed. Retrying (%d/%d)..." % (attempt, max_attempts))
