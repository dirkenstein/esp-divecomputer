#!/bin/bash
curl -u admin:$1 -i -X POST -H "Content-Type: multipart/form-data" -F "data=@$2"   http://oxygauge-webupdate.local/edit
