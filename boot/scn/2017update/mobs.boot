#MOBS
]#define BEHAVIOR_LD 0
]#define BEHAVIOR_GUARD 1
]#define BEHAVIOR_SEEK 2
]#define BEHAVIOR_WANDER 3
]#define BEHAVIOR_PILLBOX 4
]#define BEHAVIOR_SEEKING_PILLBOX 5
]#define BEHAVIOR_PLAYER_CONTROL 6
]#define BEHAVIOR_MEDIC 7
]#define BEHAVIOR_BOOMER 8
]#define PLR_AGGRO_ALL           (L)
]#define PLR_AGGRO_OTHER         (M)
]#define I			256
]#define J			512
]#define K		        1024
]#define L		 	2048
]#define M			4096
]#define TEAM_SOLO                      0
]#define TEAM_ALPHA                     1
]#define TEAM_BRAVO                     2
]#define TEAM_DELTA                     3
]#define TEAM_FOX		       4
]#define TEAM_ADMIN		       5
START
_guardian_ guardian~
the $a GUARDIAN~
HP 200000-250000
SPEED_DELAY 0
BEHAV 1
ACT BCDEM
WORTH 3
SEX 1
LOC "god-store"~
ITEMS 35 77 0

_droid_ droid battle~
a battle &gdroid&n~
HP 5000-5000
SPEED_DELAY 0
BEHAV 4
ACT BCDM
WORTH 0
SEX 0
LOC "god-store"~
ITEMS 38 79 0

tank tankMob~
a tank~
HP 29000-29000
SPEED_DELAY 0
BEHAV 6
ACT BCDM 
WORTH 3
SEX 0
LOC "god-store"~
ITEMS 207 59 11 4 0
OnCreate wield turret machine~
OnCreate load~

_trooper_ elite trooper~
an elite trooper~
HP 4000-4000
SPEED_DELAY 0
BEHAV 0
ACT 4096
WORTH 0
SEX 0
LOC "god-store"~
ITEMS 32 2 0

_medic_ medic~
a Team $a &WM&Re&Wd&Ri&Wc~
HP 6000-8000
SPEED_DELAY 0
BEHAV 7
ACT M
SEX 2   
LOC "god-store"~
ITEMS 3 2 176 176 176 176 0

_turret_ lx-810t laser turret~
an LX-810T Laser Turret~
HP 3000-6000
SPEED_DELAY 0
BEHAV 4 
ACT BCDM
WORTH 0
SEX 0
LOC "god-store"~
ITEMS 46 0

_marine_ marine~
a Team $a Marine~
HP 7000-8000  
SPEED_DELAY 1
BEHAV 2
ACT 4096
SEX 1
LOC "god-store"~
ITEMS 51 74 74 0

general pataki elite button clan~
&XGeneral Pataki&n of the &gElite &RButton&n &gClan&n~
HP 60000-70000
SPEED_DELAY 0
BEHAV 1
ACT BCDEM
WORTH 5
SEX 1
LOC random 2~
ITEMS 10 37 78 127 127 105 126 123 103 123 126 124 0

Elite Button Guard soldier~
&nan &gElite &RButton&n &gClan&n Soldier~
HP 5000-10000 
SPEED_DELAY 0
BEHAVE 1
ACT M
SEX 1
LOC rand 2~
ITEMS 125 123 126 126 32 76 0

Elite Button Guard soldier~
&nan &gElite &RButton&n &gClan&n Soldier~
HP 5000-10000
SPEED_DELAY 0  
BEHAVE 1
ACT 4096
SEX 1
LOC rand 2~
ITEMS 125 123 126 126 32 76 0

Elite Button Guard soldier~
&nan &gElite &RButton&n &gClan&n Soldier~
HP 5000-10000
SPEED_DELAY 0  
BEHAVE 1
ACT M
SEX 1
LOC rand 2~
ITEMS 125 123 126 126 32 76 0

Elite Button Guard soldier~
&nan &gElite &RButton&n &gClan&n Soldier~
HP 5000-10000
SPEED_DELAY 0  
BEHAVE 1
ACT 4096
SEX 1
LOC rand 2~
ITEMS 125 123 126 126 32 76 0

sebby~
Sebby the Pick Pocket~
HP 9000-10000 
SPEED_DELAY 1
BEHAV 2 
ACT M
WORTH 2
SEX 1
LOC rand 2~
ITEMS 53 76 133 134 135 28 0

Elite Button Guard soldier~
&nan &gElite &RButton&n &gClan&n Soldier~
HP 5000-10000
SPEED_DELAY 0
BEHAVE 1
ACT 4096
SEX 1
LOC rand 2~
ITEMS 125 123 126 126 32 76 0

elite button clan scout~
&nan &gElite &RButton&n &gClan&n Scout~
HP 5000-7000
SPEED_DELAY 0
BEHAVE 1
ACT M
SEX 1
LOC rand 2~
ITEMS 123 125 126 126 14 55 55 0

elite button clan scout~
&nan &gElite &RButton&n &gClan&n Scout~
HP 5000-7000
SPEED_DELAY 0
BEHAVE 1
ACT 4096
SEX 1
LOC rand 2~
ITEMS 123 125 126 126 14 55 55 0

_enforcer_ enforcer~
the enforcer~
HP 1000000-1000000
SPEED_DELAY 4
BEHAVE 0
ACT 0
SEX 1
LOC god gen~
ITEMS 0

operator radio~
a radio operator~
HP 4000-6000  
SPEED_DELAY 2
BEHAVE 3
ACT 0   
SEX 1
LOC random 0~   
ITEMS 3 2 2 173 174 174 174 0

operator radio~
a radio operator~
HP 4000-6000
SPEED_DELAY 2
BEHAVE 2
ACT 0
SEX 1
LOC random 0~
ITEMS 3 2 2 173 174 174 174 199 0

postal worker~
&na &Xdisgruntled &RUS &BPostal Worker&n~  
HP 8000-20000 
SPEED_DELAY 6
BEHAVE 2
ACT M
SEX 1
LOC random 0~   
ITEMS 33 73 128 129 0

elite button clan scout~
&nan &gElite &RButton&n &gClan&n Scout~
HP 5000-7000
SPEED_DELAY 0  
BEHAVE 1
ACT 4096
SEX 1
LOC rand 0~
ITEMS 123 125 126 126 14 55 55 0

elite button clan scout~
&nan &gElite &RButton&n &gClan&n Scout~
HP 5000-7000
SPEED_DELAY 0  
BEHAVE 1
ACT 4096
SEX 1
LOC rand 0~
ITEMS 123 125 126 126 14 55 55 0

SWAT TEAM sniper~
&na &XSWAT team &Bs&Xn&Bi&Xp&Be&Xr&n~
HP 17000-20000
SPEED_DELAY 0
BEHAVE 3
ACT 4096
TEAM ghost
SEX 1
LOC random 1~
ITEMS 21 67 27 116 116 117 117 118 119 120 121 122 150 150 0

space marine~
the &XSpace &RMarine&n~
HP 20000-30000 
SPEED_DELAY 0
BEHAVE 2
ACT 4096
TEAM ghost
WORTH 2
SEX 1
LOC rand 1~
ITEMS 39 80 0

suicidal teenager angst-ridden ridden~
an &Xangst&n-&Xridden &Wteenager&n~
HP 7000-8000
SPEED_DELAY 0
BEHAV 8
ACT 4096
SEX 1
LOC rand 0~
ITEMS 17 1 1 163 163 163 163 163 159 159 65 65 0

jsoh dedly bomer deadly boomer~
&cjsoh &nthe &Rdedly bomer&n~
HP 7000-8000
SPEED_DELAY 0
BEHAV 8
ACT 4096
SEX 1
LOC rand 0~
ITEMS 32 2 2 1 1 149 149 149 164 164 163 163 147 115 126 0

GZ player kamikaze t-shirt~
a &RG&XZ &nplayer wearing a &YKamikaze&n t-shirt~
HP 7000-8000
SPEED_DELAY 0
BEHAV 8
ACT 4096
SEX 1
LOC rand 0~
ITEMS 17 1 1 153 153 153 153 153 153 164 164 159 65 65 186 0

$~

#$

