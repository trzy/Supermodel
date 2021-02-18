#
# Supermodel
# A Sega Model 3 Arcade Emulator.
# Copyright 2011-2021 Bart Trzynadlowski, Nik Henson, Ian Curtis,
#                     Harry Tuttle, and Spindizzi
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
# update_model3emu_snapshot.py
#
# SVN snapshot build script. Checks latest SVN revision against Supermodel3.com
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
#
# To perform a test run:
#   - Download https://supermodel3.com/Download.html
#   - Run: python update_model3emu_snapshot.py --working-dir=c:\tmp\build --test-run
#

import argparse
import os
import shutil
import sys
import tempfile

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
    invocation = self.bash_path + " -l -c \"" + "cd " + working_dir + " && " + command + "\""
    if log:
      print("Executing: %s" % invocation)
    result = subprocess.run(invocation, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if print_output:
      print(result.stdout.decode())
    return result

def get_web_page(url, test_run):
  if test_run:
    with open("Download.html") as fp:
      html = fp.read()
  else:
    import urllib.request
    with urllib.request.urlopen("https://supermodel3.com/Download.html") as data:
      html = data.read().decode("utf-8")
  return html

def get_comment(line):
  comment_begin = line.find("<!--")
  comment_end = line.find("-->")
  if comment_begin >= 0 and comment_end > 0 and comment_end > comment_begin:
    return line[comment_begin+4:comment_end].strip()
  return None

def add_new_file(html, filename, version, svn_revision):
  # Scan for: <!-- BEGIN_SVN_SNAPSHOTS $TEMPLATE -->
  lines = html.splitlines()
  for i in range(len(lines)):
    line = lines[i]
    text = get_comment(line)
    if text and text.startswith("BEGIN_SVN_SNAPSHOTS "):
      template = text[len("BEGIN_SVN_SNAPSHOTS "):]
      new_row = template.replace("$FILENAME", filename).replace("$VERSION", version).replace("$REVISION", svn_revision)
      svn_tag = "<!-- SVN_REVISION=%s -->" % svn_revision
      lines.insert(i + 1, new_row + " " + svn_tag)
      html = "\n".join(lines)
      return html
  raise ParseError("BEGIN_SVN_SNAPSHOTS not found")

def get_uploaded_svn_revisions(html):
  # Scan for all lines with: <!-- SVN_REVISION=$REVISION -->
  revisions = []
  for line in html.splitlines():
    text = get_comment(line)
    if text and text.startswith("SVN_REVISION"):
      tokens = text.split("=")
      if len(tokens) == 2:
        revision = tokens[1]
        revisions.append(revision)
      else:
        raise ParseError("Error parsing SVN_REVISION")
  revisions.reverse()
  return revisions

def create_change_log(bash, repo_dir, file_path, uploaded_revisions, current_revision):
  from_revision = uploaded_revisions[0] if len(uploaded_revisions) > 0 else current_revision
  result = bash.execute(working_dir=repo_dir, command="svn log -r " + current_revision + ":" + from_revision)
  if result.returncode != 0:
    return PackageError("Unable to obtain SVN log")
  change_log = result.stdout.decode().strip()
  with open(file_path, "w") as fp:
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
      "        Copyright 2011-2021 Bart Trzynadlowski, Nik Henson, Ian Curtis,\n" \
      "                          Harry Tuttle, and Spindizzi\n" \
      "\n" \
      "                                SVN CHANGE LOG\n" \
      "\n" \
      "\n"
    fp.write(header)
    fp.write(change_log)

def write_html_file(html, file_path):
  with open(file_path, "w") as fp:
    fp.write(html)

def upload(html_file, zip_file_path, username, password):
  from ftplib import FTP
  ftp = FTP("supermodel3.com")
  ftp.login(username, password)
  ftp.cwd("public_html/Files/SVN_Snapshots")
  with open(zip_file_path, "rb") as fp:
    ftp.storbinary("STOR " + os.path.basename(zip_file_path), fp)
  ftp.cwd("../../")
  with open(html_file, "rb") as fp:
    ftp.storlines("STOR Download.html", fp)
  ftp.quit()

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
    import stat
    os.chmod(name, stat.S_IWRITE)
    os.remove(name)
  for root, dirs, files in os.walk(dir):  # we expect .svn to be write-protected
    if '.svn' in dirs:
      shutil.rmtree(root+'\.svn',onerror=delete_readonly)
  shutil.rmtree(dir)

def update_svn_snapshot(working_dir, username, password, test_run, make):
  failed = False
  print("Starting %s in working directory: %s" % ("test run" if test_run else "release process", working_dir))

  try:
    bash = Bash(bash_path="c:\\msys64\\usr\\bin\\bash.exe")
    repo_dir = os.path.join(working_dir, "model3emu")

    # Fetch Supermodel download page and compare most recent uploaded SVN snapshot against current SVN revision
    print("Fetching Supermodel download page...")
    html = get_web_page(url="https://supermodel3.com/Forum/Download.html", test_run=test_run)
    uploaded_revisions = get_uploaded_svn_revisions(html=html)
    last_uploaded_revision = uploaded_revisions[-1] if len(uploaded_revisions) > 0 else None
    print("Checking current SVN revision...")
    result = bash.execute(working_dir=working_dir, command="svn info --show-item revision https://svn.code.sf.net/p/model3emu/code/trunk")
    if result.returncode != 0:
      raise CheckoutError("SVN revision check failed")
    current_revision = result.stdout.decode().strip()

    # If SVN has a newer version, or if performing a test run, build it and update the web page
    if current_revision == last_uploaded_revision and (not test_run):
      print("Nothing to do. Revision already uploaded: %s" % current_revision)
    else:
      # Check out
      print("SVN revision is %s but uploaded revision is %s" % (current_revision, last_uploaded_revision))
      print("Checking out Supermodel...")
      result = bash.execute(working_dir=working_dir, command="svn co https://svn.code.sf.net/p/model3emu/code/trunk model3emu")
      if result.returncode != 0:
        raise CheckoutError("SVN checkout failed")

      # Build
      print("Building Supermodel...")
      result = bash.execute(working_dir=repo_dir, command=make + " -f Makefiles/Makefile.Win32 version")
      if result.returncode != 0:
        raise BuildError("Failed to obtain version")
      version = result.stdout.decode().strip()
      result = bash.execute(working_dir=repo_dir, command=make + " -f Makefiles/Makefile.Win32 release NET_BOARD=1")
      if result.returncode != 0:
        raise BuildError("Build failed")

      # Stage the release package files
      print("Creating release package...")
      pkg_dir = os.path.join(working_dir, "pkg")
      bash.execute(working_dir=working_dir, command="mkdir pkg && mkdir pkg/Config && mkdir pkg/NVRAM && mkdir pkg/Saves && mkdir pkg/ROMs")
      change_log_file_path = os.path.join(pkg_dir, "CHANGES.txt")
      create_change_log(bash, repo_dir=repo_dir, file_path=change_log_file_path, uploaded_revisions=uploaded_revisions, current_revision=current_revision)
      bash.execute(working_dir=working_dir, command="cp model3emu/Config/Supermodel.ini pkg/Config && cp model3emu/Config/Games.xml pkg/Config")
      bash.execute(working_dir=working_dir, command="echo NVRAM files go here. >pkg/NVRAM/DIR.txt")
      bash.execute(working_dir=working_dir, command="echo Save states go here. >pkg/Saves/DIR.txt")
      bash.execute(working_dir=working_dir, command="echo Recommended \\(but not mandatory\\) location for ROM sets. >pkg/ROMs/DIR.txt")
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
        "ROMs/DIR.txt"
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
      html = add_new_file(html=html, filename=zip_file, version=version, svn_revision=current_revision)
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
      failed = update_svn_snapshot(working_dir=options.working_dir, username=options.username, password=options.password, test_run=options.test_run, make=options.make)
    else:
      with tempfile.TemporaryDirectory() as working_dir:
        failed = update_svn_snapshot(working_dir=working_dir, username=options.username, password=options.password, test_run=options.test_run, make=options.make)
    # Retry until success
    if not failed:
      break
    attempt += 1
    if attempt <= max_attempts:
      print("Release failed. Retrying (%d/%d)..." % (attempt, max_attempts))
