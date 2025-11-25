# VR30Z33Can
VR30 to Z33 Dash CANBUS Converter


This is pretty straightforward.  Math to convert the CAN signal into something that the Z33 dash understands is odd and most likely isn't 100% correct.

There is now code for the MKR Zero series.  This code is pretty much the same as the base Arduino code, changes made to account for the different pin the MKR Zero CAN shield. Same libs work. 

Use the most recent version of the MCP2515 library from SparkFun for this to work right.

Issues: Found that the VR30 cluster generates a CAN Message on ID x355 - This is the cluster translating the vehicle speed as it saw it from the ABS unit. This message ID doesn't exist on the Z33 bus. A result of this was that the VR30 automatic transmissions were throwing a P0500 and P1271 code as there was a missing speed signal.
