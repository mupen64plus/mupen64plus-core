#!/usr/bin/env python
#/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
# *   Mupen64plus - regression-video.py                                     *
# *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
# *   Copyright (C) 2008 Richard Goedeken                                   *
# *                                                                         *
# *   This program is free software; you can redistribute it and/or modify  *
# *   it under the terms of the GNU General Public License as published by  *
# *   the Free Software Foundation; either version 2 of the License, or     *
# *   (at your option) any later version.                                   *
# *                                                                         *
# *   This program is distributed in the hope that it will be useful,       *
# *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
# *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
# *   GNU General Public License for more details.                          *
# *                                                                         *
# *   You should have received a copy of the GNU General Public License     *
# *   along with this program; if not, write to the                         *
# *   Free Software Foundation, Inc.,                                       *
# *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
# * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

from optparse import OptionParser
from threading import Thread
from datetime import date
import subprocess
import commands
import shutil
import stat
import sys
import os

# set global report string
report = "Mupen64Plus Regression Test report\n----------------------------------\n"

#******************************************************************************
# main functions
#

def main(rootdir, cfgfile, nobuild):
    global report
    # set up child directory paths
    srcdir = os.path.join(rootdir, "source")
    shotdir = os.path.join(rootdir, "current")
    refdir = os.path.join(rootdir, "reference")
    archivedir = os.path.join(rootdir, "archive")
    # run the test procedure
    tester = RegTester(rootdir, srcdir, shotdir)
    rval = 0
    while True:
        # Step 1: load the test config file
        if not tester.LoadConfig(cfgfile):
            rval = 1
            break
        # Step 2: check out from SVN
        if not nobuild:
            if not CheckoutSource(srcdir):
                rval = 2
                break
        # Step 3: run test builds
        if not nobuild:
            testlist = [ name.strip() for name in tester.generalParams["testbuilds"].split(',') ]
            makeparams = [ params.strip() for params in tester.generalParams["testbuildparams"].split(',') ]
            if len(testlist) != len(makeparams):
                report += "Config file error for test builds.  Build name list and makefile parameter list have different lengths.\n"
            testbuilds = min(len(testlist), len(makeparams))
            for i in range(testbuilds):
                buildname = testlist[i]
                buildmake = makeparams[i]
                BuildSource(srcdir, buildname, buildmake, True)
        # Step 4: build the binary for the video regression test
        if not nobuild:
            videobuild = tester.generalParams["videobuild"]
            videomake = tester.generalParams["videobuildparams"]
            if not BuildSource(srcdir, videobuild, videomake, False):
                rval = 3
                break
        # Step 5: run the tests, check the results
        if not tester.RunTests():
            rval = 4
            break
        if not tester.CheckResults(refdir):
            rval = 5
            break
        # test procedure is finished
        break
    # Step 6: send email report and archive the results
    if not tester.SendReport():
        rval = 6
    if not tester.ArchiveResults(archivedir):
        rval = 7
    # all done with test process
    return rval

#******************************************************************************
# Checkout & build functions
#

def CheckoutSource(srcdir):
    global report
    # remove any current source directory
    if not deltree(srcdir):
        return False
    # call svn to checkout current Mupen64Plus source tree
    output = commands.getoutput("svn co svn://fascination.homelinux.net:7684/mupen64plus/trunk " + srcdir)
    # parse the output
    lastline = output.split("\n")[-1]
    if lastline[:20] == "Checked out revision":
        report += "SVN Checkout successful: %s\n\n" % lastline
        return True
    report += "SVN Error: %s\n\n" % lastline
    return False

def BuildSource(srcdir, buildname, buildmake, istest):
    global report
    # print build report message and clear counters
    testbuildcommand = "make -C %s %s" % (srcdir, buildmake)
    if istest:
        report += "Running test build \"%s\" with command \"%s\"\n" % (buildname, testbuildcommand)
    else:
        report += "Building Mupen64Plus \"%s\" for video test with command \"%s\"\n" % (buildname, testbuildcommand)
    warnings = 0
    errors = 0
    # run make and capture the output
    output = commands.getoutput(testbuildcommand)
    makelines = output.split("\n")
    # print warnings and errors
    for line in makelines:
        if "error:" in line:
            report += "    " + line + "\n"
            errors += 1
        if "warning:" in line:
            report += "    " + line + "\n"
            warnings += 1
    report += "%i errors. %i warnings.\n" % (errors, warnings)
    if errors > 0 and not istest:
        return False
    # check for program files
    binfiles = [ "mupen64plus" ]
    libfiles = [ "blight_input.so", "dummyaudio.so", "dummyvideo.so", "glN64.so", "glide64.so", "ricevideo.so",
                 "mupen64_hle_rsp_azimer.so", "jttl_audio.so" ]
    filelist = [ os.path.join(srcdir, filename) for filename in binfiles ]
    filelist += [ os.path.join(srcdir, "plugins", filename) for filename in libfiles ]
    for filename in filelist:
        if not os.path.exists(filename):
            report += "Build failed: '%s' not found\n" % filename
            errors += 1
    if errors > 0 and not istest:
        return False
    # clean up if this was a test
    if istest:
        os.system("make -C %s clean" % srcdir)
    # build was successful!
    return True

#******************************************************************************
# Test execution classes
#
class RegTester:
    def __init__(self, rootdir, bindir, screenshotdir):
        self.rootdir = rootdir
        self.bindir = bindir
        self.screenshotdir = screenshotdir
        self.libdir = os.path.join(bindir, "plugins")
        self.generalParams = { }
        self.gamesAndParams = { }
        self.videoplugins = [ "glN64.so", "glide64.so", "ricevideo.so" ]
        self.thisdate = str(date.today())

    def LoadConfig(self, filename):
        global report
        # read the config file
        report += "\nLoading regression test configuration.\n"
        try:
            cfgfile = open(os.path.join(self.rootdir, filename), "r")
            cfglines = cfgfile.read().split("\n")
            cfgfile.close()
        except Exception, e:
            report += "Error in RegTestConfigParser::LoadConfig(): %s" % e
            return False
        # parse the file
        GameFilename = None
        for line in cfglines:
            # strip leading and trailing whitespace
            line = line.strip()
            # test for comment
            if len(line) == 0 or line[0] == '#':
                continue
            # test for new game filename
            if line[0] == '[' and line [-1] == ']':
                GameFilename = line[1:-1]
                if GameFilename in self.gamesAndParams:
                    report += "    Warning: Config file '%s' contains duplicate game entry '%s'\n" % (filename, GameFilename)
                else:
                    self.gamesAndParams[GameFilename] = { }
                continue
            # print warning and continue if it's not a (key = value) pair
            pivot = line.find('=')
            if pivot == -1:
                report += "    Warning: Config file '%s' contains unrecognized line: '%s'\n" % (filename, line)
                continue
            # parse key, value
            key = line[:pivot].strip().lower()
            value = line[pivot+1:].strip()
            if GameFilename is None:
                paramDict = self.generalParams
            else:
                paramDict = self.gamesAndParams[GameFilename]
            if key in paramDict:
                report += "    Warning: Game '%s' contains duplicate key '%s'\n" % (str(GameFilename), key)
                continue
            paramDict[key] = value
        # check for required parameters
        if "rompath" not in self.generalParams:
            report += "    Error: rompath is not given in config file\n"
            return False
        # config is loaded
        return True

    def RunTests(self):
        global report
        rompath = self.generalParams["rompath"]
        if not os.path.exists(rompath):
            report += "    Error: ROM directory '%s' does not exist!\n" % rompath
            return False
        # Remove any current screenshot directory
        if not deltree(self.screenshotdir):
            return False
        # Data initialization and start message
        os.mkdir(self.screenshotdir)
        for plugin in self.videoplugins:
            videoname = plugin[:plugin.find('.')]
            os.mkdir(os.path.join(self.screenshotdir, videoname))
        report += "\nRunning regression tests on %i games.\n" % len(self.gamesAndParams)
        # loop over each game filename given in regtest config file
        for GameFilename in self.gamesAndParams:
            GameParams = self.gamesAndParams[GameFilename]
            # if no screenshots parameter given for this game then skip it
            if "screenshots" not in GameParams:
                report += "    Warning: no screenshots taken for game '%s'\n" % GameFilename
                continue
            # make a list of screenshots and check it
            shotlist = [ str(int(framenum.strip())) for framenum in GameParams["screenshots"].split(',') ]
            if len(shotlist) < 1 or (len(shotlist) == 1 and shotlist[0] == '0'):
                report += "    Warning: invalid screenshot list for game '%s'\n" % GameFilename
                continue
            # run a test for each video plugin
            for plugin in self.videoplugins:
                videoname = plugin[:plugin.find('.')]
                # check if this plugin should be skipped
                if "skipvideo" in GameParams:
                    skipit = False
                    skiplist = [ name.strip() for name in GameParams["skipvideo"].split(',') ]
                    for skiptag in skiplist:
                        if skiptag.lower() in plugin.lower():
                            skipit = True
                    if skipit:
                        continue
                # construct the command line
                exepath = os.path.join(self.bindir, "mupen64plus")
                exeparms = ["--nogui", "--noosd", "--noask", "--emumode", "1" ]
                exeparms += [ "--testshots",  ",".join(shotlist) ]
                exeparms += [ "--installdir", self.bindir ]
                exeparms += [ "--sshotdir", os.path.join(self.screenshotdir, videoname) ]
                myconfig = os.path.join(self.rootdir, "config")
                if os.path.exists(myconfig):
                    exeparms += [ "--configdir", myconfig ]
                exeparms += [ "--gfx", os.path.join(self.libdir, plugin) ]
                exeparms += [ "--audio", os.path.join(self.libdir, "dummyaudio.so") ]
                exeparms += [ "--input", os.path.join(self.libdir, "blight_input.so") ]
                exeparms += [ "--rsp", os.path.join(self.libdir, "mupen64_hle_rsp_azimer.so") ]
                exeparms += [ os.path.join(rompath, GameFilename) ]
                # run it, but if it takes too long print an error and kill it
                testrun = RegTestRunner(exepath, exeparms)
                testrun.start()
                testrun.join(60.0)
                if testrun.isAlive():
                    report += "    Error: Test run timed out after 60 seconds:  '%s'\n" % " ".join(exeparms)
                    os.kill(testrun.pid, 9)
                    testrun.join(10.0)
                
        # all tests have been run
        return True                

    def CheckResults(self, refdir):
        global report
        # print message
        warnings = 0
        errors = 0
        report += "\nChecking regression test results\n"
        # get lists of files in the reference folders
        refshots = { }
        if not os.path.exists(refdir):
            os.mkdir(refdir)
        for plugin in self.videoplugins:
            videoname = plugin[:plugin.find('.')]
            videodir = os.path.join(refdir, videoname)
            if not os.path.exists(videodir):
                os.mkdir(videodir)
                refshots[videoname] = [ ]
            else:
                refshots[videoname] = [ filename for filename in os.listdir(videodir) ]
        # get lists of files produced by current test runs
        newshots = { }
        for plugin in self.videoplugins:
            videoname = plugin[:plugin.find('.')]
            videodir = os.path.join(self.screenshotdir, videoname)
            if not os.path.exists(videodir):
                newshots[videoname] = [ ]
            else:
                newshots[videoname] = [ filename for filename in os.listdir(videodir) ]
        # make list of matching ref/test screenshots, and look for missing reference screenshots
        checklist = { }
        for plugin in self.videoplugins:
            videoname = plugin[:plugin.find('.')]
            checklist[videoname] = [ ]
            for filename in newshots[videoname]:
                if filename in refshots[videoname]:
                    checklist[videoname] += [ filename ]
                else:
                    report += "    Warning: reference screenshot '%s/%s' missing. Copying from current test run\n" % (videoname, filename)
                    shutil.copy(os.path.join(self.screenshotdir, videoname, filename), os.path.join(refdir, videoname))
                    warnings += 1
        # look for missing test screenshots
        for plugin in self.videoplugins:
            videoname = plugin[:plugin.find('.')]
            for filename in refshots[videoname]:
                if filename not in newshots[videoname]:
                    report += "    Error: Test screenshot '%s/%s' missing.\n" % (videoname, filename)
                    errors += 1
        # do image comparisons
        for plugin in self.videoplugins:
            videoname = plugin[:plugin.find('.')]
            for filename in checklist[videoname]:
                refimage = os.path.join(refdir, videoname, filename)
                testimage = os.path.join(self.screenshotdir, videoname, filename)
                diffimage = os.path.join(self.screenshotdir, videoname, os.path.splitext(filename)[0] + "_DIFF.png")
                cmd = ("/usr/bin/compare", "-metric", "PSNR", refimage, testimage, diffimage)
                pipe = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT).stdout
                similarity = pipe.read().strip()
                pipe.close()
                try:
                    db = float(similarity)
                except:
                    db = 0
                if db > 60.0:
                    os.unlink(diffimage)
                else:
                    report += "    Warning: test image '%s/%s' does not match reference.  PSNR = %s\n" % (videoname, filename, similarity)
                    warnings += 1
        # give report and return
        report += "%i errors. %i warnings.\n" % (errors, warnings)
        return True

    def SendReport(self):
        global report
        # if there are no email addresses in the config file, then just we're done
        if "sendemail" not in self.generalParams:
            return True
        if len(self.generalParams["sendemail"]) < 5:
            return True
        # construct the email message header
        emailheader = "To: %s\n" % self.generalParams["sendemail"]
        emailheader += "From: Mupen64Plus-Tester@fascination.homelinux.net\n"
        emailheader += "Subject: %s Regression Test Results for Mupen64Plus\n" % self.thisdate
        emailheader += "Reply-to: do-not-reply@fascination.homelinux.net\n"
        emailheader += "Content-Type: text/plain; charset=UTF-8\n"
        emailheader += "Content-Transfer-Encoding: 8bit\n\n"
        # open a pipe to sendmail and dump our report
        try:
            pipe = subprocess.Popen(("/usr/sbin/sendmail", "-t"), stdin=subprocess.PIPE).stdin
            pipe.write(emailheader)
            pipe.write(report)
            pipe.close()
        except Exception, e:
            report += "Exception encountered when calling sendmail: '%s'\n" % e
            report += "Email header:\n%s\n" % emailheader
            return False
        return True

    def ArchiveResults(self, archivedir):
        global report
        # create archive dir if it doesn't exist
        if not os.path.exists(archivedir):
            os.mkdir(archivedir)
        # move the images into a subdirectory of 'archive' given by date
        subdir = os.path.join(archivedir, self.thisdate)
        if os.path.exists(subdir):
            if not deltree(subdir):
                return False
        shutil.move(self.screenshotdir, subdir)
        # copy the report into the archive directory
        f = open(os.path.join(archivedir, "report_%s.txt" % self.thisdate), "w")
        f.write(report)
        f.close()
        # archival is complete
        return True


class RegTestRunner(Thread):
    def __init__(self, exepath, exeparms):
        self.exepath = exepath
        self.exeparms = exeparms
        self.pid = 0
        self.returnval = None
        Thread.__init__(self)

    def run(self):
        # start the process
        testprocess = subprocess.Popen([self.exepath] + self.exeparms)
        # get the PID of the new test process
        self.pid = testprocess.pid
        # wait for the test to complete
        self.returnval = testprocess.wait()


#******************************************************************************
# Generic helper functions
#

def deltree(dirname):
    global report
    if not os.path.exists(dirname):
        return True
    try:
        for path in (os.path.join(dirname, filename) for filename in os.listdir(dirname)):
            if os.path.isdir(path):
                if not deltree(path):
                    return False
            else:
                os.unlink(path)
        os.rmdir(dirname)
    except Exception, e:
        report += "Error in deltree(): %s\n" % e
        return False

    return True

#******************************************************************************
# main function call for standard script execution
#

if __name__ == "__main__":
    # parse the command-line arguments
    parser = OptionParser()
    parser.add_option("-n", "--nobuild", dest="nobuild", default=False, action="store_true",
                      help="Assume source code is present; don't check out and build")
    parser.add_option("-t", "--testpath", dest="testpath",
                      help="Set root of testing directory to PATH", metavar="PATH")
    parser.add_option("-c", "--cfgfile", dest="cfgfile", default="daily-tests.cfg",
                      help="Use regression test config file FILE", metavar="FILE")
    (opts, args) = parser.parse_args()
    # check test path
    if opts.testpath is None:
        # change directory to the directory containing this script and set root test path to "."
        scriptdir = os.path.dirname(sys.argv[0])
        os.chdir(scriptdir)
        rootdir = "."
    else:
        rootdir = opts.testpath
    # call the main function
    rval = main(rootdir, opts.cfgfile, opts.nobuild)
    sys.exit(rval)


