"""
This is a copy of the jsonrpc cgihandler.py, but modified so that it uses strings
in function calls/returns instead of reading/writing to stdin and stdout. This is
so that I can get/put the input/output wherever I want.

The aim is to have a mini-server running that listens on a Unix socket and then
replies on a Unix socket. The actual CGI script will pass the request on to this
socket. This is so that I can run a program that has a persistent state. The CGI
script will be run in a new process for every request, and so can't have persistent
state.

@author Mark Grimes (mark.grimes@bristol.ac.uk) but almost no original work
@date 17/Jan/2013

I'll include the licence information from the cgihandler.py below:

  Copyright (c) 2006 Jan-Klaas Kollhof

  This file is part of jsonrpc.

  jsonrpc is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  This software is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this software; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
"""

from jsonrpc import SimpleServiceHandler
import sys,os


class CGIHandlerFromStrings(SimpleServiceHandler):
    def __init__(self, service, messageDelimiter="\n"):
        self.sendData =[]
        SimpleServiceHandler.__init__(self, service, messageDelimiter=messageDelimiter)


    def send(self, data):
        self.sendData.append(data)

    def handle(self,data):
        ## This is the input part I've changed. My version takes the data string straight
        ## as a parameter instead of reading it from stdin
        #try:
        #	contLen=int(os.environ['CONTENT_LENGTH'])
        #    data = sys.stdin.read(contLen)
        #except:
        #	data = ""
        ##execute the request
    	self.sendData =[] # Not sure why, but the previous response gets stored here and added. Not what I want.
        self.handlePartialData(data)
        reply=self.sendReply()
        self.close()
        return reply

    def sendReply(self):
        data = "\n".join(self.sendData)
        response = "Content-Type: application/json\n"
        response += "Content-Length: %d\n\n" % len(data)
        response += data

        #on windows all \n are converted to \r\n if stdout is a terminal and  is not set to binary mode :(
        #this will then cause an incorrect Content-length.
        #I have only experienced this problem with apache on Win so far.
        if sys.platform == "win32":
            import  msvcrt
            msvcrt.setmode(sys.stdout.fileno(), os.O_BINARY)
        #put out the response
        ## This is the output part I've changed. I'll return the string instead of printing it.
        #sys.stdout.write(response)
        return response


## Also removed this function so it's no longer useful.
#def handleCGIRequest(service):
#	CGIHandler(service,messageDelimiter="\n").handle()
