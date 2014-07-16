CBC analysis test stand user interface
======================================

This user interface is browser based, using AJAX communicating to a python backend. The javascript is all created automatically from python files using pyjamas http://pyjs.org. Once pyjamas is installed, cd to this directory and build

    cd $CMSSW_BASE/src/SLHCUpgradeTracker/CBCAnalysis/gui
    pyjsbuild index.py

This will create the directory "output", which needs to be served with a HTTP server that supports python CGI. Apache HTTP is probably the most common. To use apache, make sure it's installed (often called "httpd" in package managers). Then copy the configuration file here (cbcTestStand.conf) to the configuration folder and start the server.
In Scientific Linux (e.g. the Strasbourg Glib virtual machine) this would look like:

    sudo yum install httpd
    sudo cp $CMSSW_BASE/src/XtalDAQ/OnlineCBCAnalyser/gui/cbcTestStand.conf /etc/httpd/conf.d/
    chmod o+r $HOME        # Allow apache to read this directory. By default it can't read this one.
    sudo /sbin/service httpd start

The interface will then be available from a modern browser pointed at "<DAQ PC IP address>/cbcTestStand". If running in the Strasbourg virtual machine, by default this would be "http://192.168.47.128/cbcTestStand".

Original goals were:
 1) Keep all work done in a python script backend so advanced users can create their own custom scripts.
 2) Be cross platform.
 3) Be scalable for multiple CBCs

I originally thought writing in pyjamas would be good because it would fulfil all 3 simply. In retrospect it's a bit over complicated though. Interfacing to the python backend didn't work (even though it's written in python it's translated to javascript) so it has to be done through Remote Procedure Calls.