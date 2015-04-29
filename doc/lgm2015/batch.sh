# From Twitter, Facebook, imgurl, GoogleDrive, self-hosted etc
IMAGES=(
  http://the-grid-user-content.s3-us-west-2.amazonaws.com/5088bd49-fc5c-40dc-969b-1b8d8be15953.jpg
  https://sixby6.files.wordpress.com/2014/11/art-kane-helmet.jpg
  http://the-grid-user-content.s3-us-west-2.amazonaws.com/8b64248b-3512-4d94-aa1d-e7273b29ab67.jpg
)
# Server config
SERVER=localhost:8080
APIKEY=''
APISECRET=''

# Processing config
GRAPH=passthrough
WIDTH=200

i=0
for INPUT_URL in "${IMAGES[@]}"
do
  i=$((i+1))
  OUTNAME="output-"${i}.jpg
  echo "write to $OUTNAME"
  curl --output ${OUTNAME} -L --get ${SERVER}/graph/${GRAPH} --data-urlencode "input=${INPUT_URL}" --data-urlencode "width=${WIDTH}"
done

