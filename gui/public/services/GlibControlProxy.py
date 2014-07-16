#!/usr/local/bin/python

# RPC service that takes any commands and pipes them through to another script, listens for
# for the response and then returns it. This is all done over Unix sockets. If the script
# that it wants to pipe to isn't running, then it starts it.
#
# This is a bit over complicated, but apache starts this script in a new process for each
# request. Hence it cannot have persistent state. To get around this it starts the other
# script which runs permanently and communicates with that.
#
# @author Mark Grimes (mark.grimes@bristol.ac.uk)
# @date 17/Jan/2014

import os, inspect
# The "os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))" part of
# this line gets the directory of this file. I then look three parents up to get the directory
# of the CBCAnalysis installation.
INSTALLATION_PATH = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe()))), os.pardir, os.pardir, os.pardir))

if __name__ == '__main__':
	#
	# this is if JSONService.py is run as a CGI
	#

	#---------------------------------------------------
	# The important settings.
	#---------------------------------------------------
	logging=False      # Whether to dump debugging information to a log.
	sendAddress="/tmp/CBCTestStand_rpc_server"  # The socket address that the receiving script listens on
	receivingScript=INSTALLATION_PATH+"/gui/serverProcess/GlibControlService.py" # The script that will answer my requests
	#---------------------------------------------------
	# Important settings above.
	#---------------------------------------------------
	
	import socket, os, sys, time

	client = socket.socket( socket.AF_UNIX, socket.SOCK_DGRAM )
	try :
		#
		# See if I can connect to the sendAddress. If I can't then the listening script isn't
		# running yet. I could check os.path.exists( sendAddress ) to see if the socket is open,
		# but in some extreme circumstances the listening script dies before it has the chance
		# to remove the socket. In this case I'll try and remove it and start the listening
		# script.
		#
		client.connect( sendAddress )
	except socket.error :
		try :
			os.remove( sendAddress )
		except OSError as exception :
			if exception.strerror=="Operation not permitted" :
				# The socket has probably already been created by another user.
				# I'll raise a more meaningful error instead.
				raise RuntimeError( "The socket '"+sendAddress+"' could not be contacted, and could not be deleted. Is it in use by another user? You need to close and delete this socket.")
			if exception.strerror!="No such file or directory" :
				# If it doesn't exist that's fine, the socket.error was probably
				# because the listening script isn't running. Any other error I
				# want to pass on however.
				raise
		# I should be clear to start the listening script now.
		import subprocess
		devnull=open("/dev/null")
		subprocess.Popen( ['python2.6',receivingScript], stdout=devnull )
		time.sleep(1) # Sleep for a second to allow the new process to open the port
		# Now everything should be set up for me to try to connect again. If this
		# doesn't work I don't know what to do.
		client.connect( sendAddress )
		
	# Create a socket for the other script to pass the message back on. I need
	# some unique name so use the process ID.
	listeningAddress="/tmp/CBCTestStand_rpc_server_response-"+str(os.getpid())
	response = socket.socket( socket.AF_UNIX, socket.SOCK_DGRAM )
	response.bind( listeningAddress )

	# Apache should have set 'CONTENT_LENGTH' to the size of the message.
	# Read the whole message in.
	contLen=int(os.environ['CONTENT_LENGTH'])
	data = sys.stdin.read(contLen)
	if logging:
		logFile=open('/tmp/proxyDumpFile.log','a')
		logFile.write(data+"\n")
	# For the listening script to be able to read the data, I first need to let
	# it now how long the data is. I also need to tell it where to communicate
	# the response to.
	data=listeningAddress+"\n"+str(len(data))+"\n"+data
	client.send(data)
	
	packetSize=1024 # The size of the chunks I receive on the pipe
	# First find out how long the response is
	datagram = response.recv( packetSize, socket.MSG_PEEK ) # Look but don't remove
	firstNewlinePosition=datagram.find('\n')
	dataLength=int(datagram[0:firstNewlinePosition])
	messageLength=dataLength+firstNewlinePosition+1
	# Make sure the packet size is large enough to read the whole message.
	while packetSize < messageLength : packetSize=packetSize*2 # keep as a power of 2
	# Now that I have the correct packet size, I can get the full message and remove
	# it from the queue.
	datagram = response.recv( packetSize )
	message=datagram[firstNewlinePosition+1:]
	if logging:
		logFile.write(message)

	# Write the response to stdout. Apache passes this back to the requestor.
	sys.stdout.write(message)


	if logging: logFile.close()
	
	client.close()
	response.close()
	os.remove(listeningAddress)

else:
	# this is if JSONService.py is run from mod_python:
	# rename .htaccess.mod_python to .htaccess to activate,
	# and restart Apache2
	raise Exception( "I haven't figured out how to run this from mod_python yet")
	#from jsonrpc.apacheServiceHandler import handler
