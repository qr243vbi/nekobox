#!/bin/python3
from copr import v3; 
import os
login=os.getenv("COPR_LOGIN")
token=os.getenv("COPR_TOKEN")
client = v3.Client({
   'copr_url': 'https://copr.fedorainfracloud.org', 
   'login': login, 
   'token': token
})
print(client.package_proxy.build('qr243vbi', 'NekoBox', 'NekoBox'))
