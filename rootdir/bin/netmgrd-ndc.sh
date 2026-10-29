#!/system/bin/sh
# netmgrd adds its reverse rmnet links to a network while they are down, which
# netd now rejects. Defer that add until netmgrd routes the link, once it is up.

ndc=/system/bin/ndc-wrapper-1.0

case "$1 $2 $3 $5" in
    "network interface add r_rmnet_"*)
        echo "200 0 success"
        exit 0
        ;;
    "network route add r_rmnet_"*)
        $ndc network interface add "$4" "$5" > /dev/null
        ;;
esac

exec $ndc "$@"
