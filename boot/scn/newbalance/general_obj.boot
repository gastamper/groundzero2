#OBJECTS

ITEM_TYPE 5
WEAR 0
Capacity	4
Interior	The Interior of a Tank~
StartActor	The engine growls as you fire it up.~
StartInterior	The engine of the tank growls as $n fires it up.~
StartExterior	You hear the growl of the tank engine firing up.~
StopActor	The tank stops cold as you kill the engine.~
StopInterior	The tank stops cold as $n kills the engine.~
StopExterior	The tank stops growling as its engine is killed.~
Positions	ABCD
ReqPositions	C
TurretDistance  2 5
ToMob 		tankMob~
GEN O
DeployDelay	900
DeployTo	rand 0~
EXTRACT 0
USE 0
tank~
a tank~
A tank with the words "have a nice day" inscribed on the side is here.~
REPOP 100 2-2
CHSH 29000 450


ITEM_TYPE 0
WEAR 1
GEN 0
EXTRACT 0
USE G
fire extinguisher~
a fire extinguisher~
A fire extinguisher lies here.~
REPOP 85 2-3 
CHSH 2000 500


ITEM_TYPE 0
WEAR 1
GEN 0
EXTRACT 0
USE D
radio equipment evac~
an emergency evac radio~
a radio is buried in the dust here~
REPOP 95 3-4
CHSH 1000 1000

ITEM_TYPE 0
WEAR 1
GEN 0
EXTRACT 0
USE C
radio airforce~
an airforce radio~
a radio is buried in the dust here.~
REPOP 95 3-4
CHSH 1000 1000

ITEM_TYPE 0
WEAR 4097
GEN C
EXTRACT 0
USE 0
condition monitor equipment wristband~
a condition monitor and homing beacon~
a small gold wristband is here~
REPOP 60 1-1
CHSH 5000 5000


ITEM_TYPE 0
WEAR 1
GEN 0
EXTRACT 0
USE A
DAM_CH 2000 0 0
DAM_ST 0 0 0
med pack medical medkit kit first heal~
a &Rred&n cross first-aid kit~
a &Wwhite&n box with a &Rred&n cross lies here.~
REPOP 99 12-20
CHSH 5000 5000


ITEM_TYPE 0
WEAR 1
GEN 0 
EXTRACT 0     
USE A    
DAM_CH 6000 0 0
DAM_ST 0 0 0
heal booze glass martini alcohol~
a glass of martini~
A full glass of martini sits here.~
REPOP 95 1-1
CHSH 5000 5000

ITEM_TYPE 0
WEAR 1
GEN 0
EXTRACT 0
USE E
droid construction set~
a droid construction set~
A droid construction set lies here.~
REPOP 0
CHSH 4000 4000

ITEM_TYPE 0
WEAR 1
GEN 0
EXTRACT 0
USE F
seeker droid construction set~
a construction set for a seeker droid~
A construction set for a seeker droid lies here.~
REPOP 75 2-2 
CHSH 4000 4000

ITEM_TYPE 0
WEAR 1
GEN 0
EXTRACT 0     
USE I
turret LX 810T construction set kit~
an LX-810T construction set~
A kit for an LX-810T photon turret lies here.~
REPOP 100 1-1
CHSH 4000 4000

ITEM_TYPE 0
WEAR 32769
GEN G
EXTRACT 0
USE 0
metal detector~
a metal detector~
A metal detector lies here.~
REPOP 90 1-1
CHSH 4000 4000

ITEM_TYPE 0  
WEAR 1
GEN 0
EXTRACT 0
USE K
MAX_USES 3
lifeforce scanner~
a lifeforce scanner~
A complex piece of scanning equipment lies here.~
REPOP 90 1-1 
CHSH 250 250 

ITEM_TYPE 2
WEAR 17
ARMOR_VAL 5
GEN N
EXTRACT 0
USE 0
armor pair earmuffs~
a pair of earmuffs~
Someone dropped some earmuffs here.~
REPOP 95 5-10
CHSH 20 20

ITEM_TYPE 3
WEAR 1
RANGE 0
GEN B
EXTRACT G
an early detection system~
USE 0
eds early detection system device antenna~
an early detection system~
A small round device with an antenna lies here.~
REPOP 100 1-3
CHSH 500 500

$

#$
