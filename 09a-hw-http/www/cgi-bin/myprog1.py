#!/usr/bin/python
import os
# collect request body from stdin
inlen=os.getenv('CONTENT_LENGTH')
intxt=os.read(0,int(inlen)).decode('ascii')
# assemble response body
outbody="Hello CS324\n"
outbody+="Query string: %s\n"%os.getenv('QUERY_STRING')
outbody+="Request body: %s\n"%intxt
# prepend HTTP headers
outlen=len(outbody)
outtxt="Content-Type: text/plain\r\n"
outtxt+="Content-Length: %d\r\n\r\n"%outlen
outtxt+=outbody
# send to stdout
os.write(1,str.encode(outtxt))
