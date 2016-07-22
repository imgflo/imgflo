OS=`uname`

if [[ $OS = "Darwin" ]]
then
    brew install pkg-config
    brew install glib json-glib
else
    echo Travis apt addon should take care of this
fi
