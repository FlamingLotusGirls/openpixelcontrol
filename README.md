FLG's fork of OPC
=================

This is the Flaming Lotus Girls fork of Open Pixel Control, which has been
modified for use with our art piece, [Soma][soma], currently on permanent
installation along the [San Francisco waterfront][pier14].  If you're
interested in OPC in general and not specifically for use with Soma, you
probably want the usptram version, at
[https://github.com/zestyping/openpixelcontrol][opc].  For more information
about using OPC to develop patterns for Soma, please see
[https://github.com/FlamingLotusGirls/soma/tree/master/pier14/opc-client][opc-client].

[opc]:         https://github.com/zestyping/openpixelcontrol
[soma]:        http://flaminglotus.com/art/soma/
[pier14]:      http://flaminglotus.com/soma-on-pier-14/
[opc-client]:  https://github.com/FlamingLotusGirls/soma/tree/master/pier14/opc-client

## Set initial camera position

It can be really annoying to move and pan around every time you run the simulator

1. run with verbose flag `bin/gl_server ... --verbose`

  prints `x y z elv dst asp ang: 0.00 -49.00 0.00 66.00 302.40 1.33 1078.00` each time you move the camera

2. pass values in the next run

  `bin/gl_server ... --initcam='0.00 -49.00 0.00 66.00 302.40 1.33 1078.00'

3. scene will load wherever you left off. happiness.