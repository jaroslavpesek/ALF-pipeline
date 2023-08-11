#!/usr/bin/python3
# vim: tabstop=8 expandtab shiftwidth=4 softtabstop=4

import pytrap
import sys
import optparse
import json
import re
from optparse import OptionParser
import requests

parser = OptionParser(add_help_option=True)
parser.add_option("-i", "--ifcspec", dest="ifcspec",
        help="TRAP IFC specifier", metavar="IFCSPEC")
parser.add_option("-n", "--no-eos", action="store_true",
        help="Don't send end-of-stream message at the end.")
parser.add_option("-M", "--maps", dest="maps", metavar="JSONFILE",
        help="Load mappings and apply them on incoming messages.")
parser.add_option("-u", "--URL", dest="URL", default="http://localhost:5000",
        help="URL of DP3 API.")
parser.add_option("-v", "--verbose", action="store_true",
        help="Set verbose mode - print debug messages.")
parser.add_option("-f", "--format", dest="format", default="",
    help="Set format identifier string (this must match the format ID required by receiving module)", metavar="FMT_ID")

# Parse remaining command-line arguments
(options, args) = parser.parse_args()

trap = pytrap.TrapCtx()
trap.init(sys.argv, 1, 0)

trap.setRequiredFmt(0, pytrap.FMT_UNIREC)

json_datapoints = [{
  "type": "flow",
  "id": 33,
  "attr": "features",
  "v": "s",
  "src": "salf"
},
{
  "type": "flow",
  "id": 33,
  "attr": "characteristics",
  "v": "s",
  "src": "salf"
},{
  "type": "flow",
  "id": 33,
  "attr": "state",
  "v": "new",
  "src": "salf"
}]

datapointID =100000 #first id TODO
URL= "http://localhost:5000"

if options.URL:
    URL = options.URL

if not URL.startswith("https:") or not URL.startswith("http:"):
    u = "http://"
    u +=URL
    URL = u

if(URL[-1] != '/'):
    URL +='/'

if options.verbose:
    print("DP3 API test: " + URL)

headers = {'Accept': 'application/json'}
   
try:
    r = requests.get(url=URL,headers=headers,timeout=5) #TODO timeout
except requests.exceptions.RequestException as e:
    raise SystemExit(e)


if(r.json()['detail'] != "It works!"):
    print("Error: dp3 API not found on URL\n")
    sys.exit()

URL +='datapoints'

session = requests.Session()
session.headers ={'Content-Type': 'application/json'}

def default(o):
    if isinstance(o, pytrap.UnirecIPAddr):
        return str(o)
    elif isinstance(o, pytrap.UnirecTime):
        return float(o)
    else:
        return repr(o)

mapping = None

if options.maps:
    with open(options.maps, "r") as f:
        mapping = json.load(f)

def apply_mappings(data, mapping):
    matches = mapping.get("match", None)
    regexs = mapping.get("regex", None)
    timify = mapping.get("timify", None)
    if matches:
        for field in matches:
            data[field["dest"]] = field["mapping"].get(str(data[field["src"]]), None)

    if regexs:
        for field in regexs:
            for reg in field["mapping"].keys():
                if re.match(reg, str(data[field["src"]])):
                    data[field["dest"]] = field["mapping"][reg]
    if timify:
        for t in timify:
            data[t] = pytrap.UnirecTime(float(data[t])).format()
    return data

# Main loop
stop = False
while not stop:
    try:
        data = trap.recv()
    except pytrap.FormatChanged as e:
        fmttype, inputspec = trap.getDataFmt(0)
        rec = pytrap.UnirecTemplate(inputspec)
        data = e.data
    if len(data) <= 1:
        # Send end-of-stream message and exit
        if not options.no_eos:
            trap.send(bytearray(b"0"))
            trap.sendFlush(0)
        break
    rec.setData(data)
    if mapping:
        d = apply_mappings(rec.getDict(), mapping)
    else:
        d = rec.getDict()

    if options.verbose:
        print(json.dumps(d, default=default))

    for i in range(3):
        json_datapoints[i]['id'] = datapointID
    datapointID+=1
    
    k = rec.getDict()
    tmp = dict()
    tmp['PREDICTED_PROBAS']=k['PREDICTED_PROBAS']
    k.pop('PREDICTED_PROBAS')

    json_datapoints[0]['v'] =json.dumps(k,default=default)
    json_datapoints[1]['v'] =json.dumps(tmp,default=default)

    if options.verbose:
        print("Sending data: " + json.dumps(json_datapoints))
    
    try:
        r = session.post(url=URL,json=json_datapoints,timeout=10)
    except requests.exceptions.RequestException as e:
        print("Error exception POST")
        print("Last ID: "+str(datapointID))
        raise SystemExit(e)
    
    if options.verbose:
        print(r.status_code)
        print(r.headers) #TODO remove ?
        print(r.json())


# Free allocated TRAP IFCs
trap.finalize()

