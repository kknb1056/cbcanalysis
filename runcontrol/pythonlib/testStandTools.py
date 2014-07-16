import os

def ping( hostname ) :
	response = os.system("ping -c 1 -w2 " + hostname + " > /dev/null 2>&1")
	if response == 0 : return True
	else : return False