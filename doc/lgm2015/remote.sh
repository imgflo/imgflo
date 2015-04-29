
INPUT_URL="https://sixby6.files.wordpress.com/2014/11/art-kane-helmet.jpg"
SERVER=localhost:8080
GRAPH=passthrough
curl -G -v -L --output processed.jpg --data-urlencode "input=${INPUT_URL}" ${SERVER}/graph/${GRAPH}

# 
