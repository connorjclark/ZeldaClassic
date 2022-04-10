#include "comboeditor.h"
#include "info.h"
#include "alert.h"
#include "../zsys.h"
#include "../tiles.h"
#include <gui/builder.h>

extern bool saved;
extern zcmodule moduledata;
extern newcombo *combobuf;
extern comboclass *combo_class_buf;
extern int32_t CSet;
char *ordinal(int32_t num);
using std::string;
using std::to_string;

static size_t cmb_tab1 = 0, cmb_tab2 = 0;

static bool edited = false, cleared = false;
bool call_combo_editor(int32_t index)
{
	int32_t cs = CSet;
	edited = false; cleared = false;
	ComboEditorDialog(index).show();
	while(cleared)
	{
		cleared = false;
		ComboEditorDialog(index, true).show();
	}
	if(!edited) CSet = cs;
	return edited;
}

ComboEditorDialog::ComboEditorDialog(newcombo const& ref, int32_t index, bool clrd):
	local_comboref(ref), index(index),
	list_ctype(GUI::ListData::combotype(true)),
	list_flag(GUI::ListData::mapflag(true)),
	list_combscript(GUI::ListData::combodata_script()),
	list_counters(GUI::ListData::counters()),
	list_sprites(GUI::ListData::miscsprites()),
	list_weaptype(GUI::ListData::lweaptypes()),
	list_deftypes(GUI::ListData::deftypes()),
	lasttype(-1), typehelp(""), flaghelp("")
{
	if(clrd)
	{
		word foo = local_comboref.foo; //Might need to store this?
		local_comboref.clear();
		local_comboref.foo = foo;
	}
}

ComboEditorDialog::ComboEditorDialog(int32_t index, bool clrd):
	ComboEditorDialog(combobuf[index], index, clrd)
{}

//{ Help Strings
static const char *combotype_help_string[cMAX] =
{
	"Select a Type, then click this button to find out what it does.",
	"The player is warped via Tile Warp A if they step on the bottom half of this combo.",
	"The player marches down into this combo and is warped via Tile Warp A if they step on this. The combo's tile will be drawn above the player during this animation.",
	"Liquid can contain Zora enemies and can be crossed with various weapons and items. If the matching quest rule is set, the player can drown in it.",
	"When touched, this combo produces an Armos and changes to the screen's Under Combo.",
	"When touched, this combo produces one Ghini.",
	"Raft paths must begin on a Dock-type combo. (Use the Raft combo flag to create raft paths.)",
	"", //cUNDEF
	"A Bracelet is not needed to push this combo, but it can't be pushed until the enemies are cleared from the screen.",
	"A Bracelet is needed to push this combo. The screen's Under Combo will appear beneath it when it is pushed aside.",
	"A Bracelet is needed to push this combo, and it can't be pushed until the enemies are cleared from the screen.",
	"If the 'Statues Shoot Fire' Screen Data flag is checked, an invisible fireball shooting enemy is spawned on this combo.",
	"If the 'Statues Shoot Fire' Screen Data flag is checked, an invisible fireball shooting enemy is spawned on this combo.",
	"The player's movement speed is reduced while they walk on this combo. Enemies will not be affected.",
	"While the player is standing on top of this, they will be moved upward at 2 pixels per 3 frames (or some dir at a custom-set speed), until they collide with a solid combo.",
	"While the player is standing on top of this, they will be moved downward at 2 pixels per 3 frames (or some dir at a custom-set speed), until they collide with a solid combo.",
	"While the player is standing on top of this, they will be moved leftward at 2 pixels per 3 frames (or some dir at a custom-set speed), until they collide with a solid combo.",
	"While the player is standing on top of this, they will be moved rightward at 2 pixels per 3 frames (or some dir at a custom-set speed), until they collide with a solid combo.",
	"The player is warped via Tile Warp A if they swim on this combo. Otherwise, this is identical to Water.",
	"The player is warped via Tile Warp A if they dive on this combo. Otherwise, this is identical to Water.",
	"If this combo is solid, the Ladder and Hookshot can be used to cross over it. It only permits the Ladder if it's on Layer 0.",
	"This triggers Screen Secrets when the bottom half of this combo is stepped on, but it does not set the screen's 'Secret' Screen State.",
	"This triggers Screen Secrets when the bottom half of this combo is stepped on, and sets the screen's 'Secret' Screen State, making the secrets permanent.",
	"", // Unused
	"When stabbed or slashed with a Sword, this combo changes into the screen's Under Combo.",
	"Identical to Slash, but an item from Item Drop Set 12 is created when this combo is slashed.",
	"A Bracelet with a Push Combo Level of 2 is needed to push this combo. Otherwise, this is identical to Push (Heavy).",
	"A Bracelet with a Push Combo Level of 2 is needed to push this combo. Otherwise, this is identical to Push (Heavy, Wait).",
	"When hit by a Hammer, this combo changes into the next combo in the list.",
	"If this combo is struck by the Hookshot, the player is pulled towards the combo.",
	"", //cHSBRIDGE (Unimplemented)
	// Damage Combos
	"",
	"",
	"",
	"",
	// Anyway...
	"If the 'Statues Shoot Fire' Screen Data flag is checked, an invisible fireball shooting enemy is spawned on this combo.",
	"This flag is obsolete. It behaves identically to Combo Flag 32, Trap (Horizontal, Line of Sight).",
	"This flag is obsolete. It behaves identically to Combo Flag 33, Trap (Vertical, Line of Sight).",
	"This flag is obsolete. It behaves identically to Combo Flag 34, Trap (4-Way, Line of Sight).",
	"This flag is obsolete. It behaves identically to Combo Flag 35, Trap (Horizontal, Constant).",
	"This flag is obsolete. It behaves identically to Combo Flag 36, Trap (Vertical Constant).",
	"The player is warped via Tile Warp A if they touch any part of this combo, but their on-screen position remains the same. Ground enemies can't enter.",
	"If this combo is solid, the Hookshot can be used to cross over it.",
	"This combo's tile is drawn between layers 3 and 4 if it is placed on layer 0.",
	"Flying enemies (Keese, Peahats, Moldorms, Patras, Fairys, Digdogger, Manhandla, Ghinis, Gleeok heads) can't fly over or appear on this combo.",
	"Wand magic and enemy magic that hits  this combo is reflected 180 degrees, and becomes 'reflected magic'.",
	"Wand magic and enemy magic that hits  this combo is reflected 90 degrees, and become 'reflected magic'.",
	"Wand magic and enemy magic that hits  this combo is reflected 90 degrees, and become 'reflected magic'.",
	"Wand magic and enemy magic that hits  this combo is duplicated twice, causing three shots to be fired in three directions.",
	"Wand magic and enemy magic that hits  this combo is duplicated thrice, causing four shots to be fired from each direction.",
	"Wand magic and enemy magic that hits this combo is destroyed.",
	"The player marches up into this combo and is warped via Tile Warp A if they step on this. The combo's tile will be drawn above the player during this animation.",
	"The combo's tile changes depending on the player's position relative to the combo. It uses eight tiles per animation frame.",
	"Identical to Eyeball (8-Way A), but the angles at which the tile will change are offset by 22.5 degrees (pi/8 radians).",
	"Tektites cannot jump through or appear on this combo.",
	"Identical to Slash->Item, but when it is slashed, Bush Leaves sprites are drawn and the 'Tall Grass slashed' sound plays.",
	"Identical to Slash->Item, but when it is slashed, Flower Clippings sprites are drawn and the 'Tall Grass slashed' sound plays.",
	"Identical to Slash->Item, but when it is slashed, Grass Clippings sprites are drawn and the 'Tall Grass slashed' sound plays.",
	"Ripples sprites are drawn on the player when they walk on this combo. Also, Quake Hammer pounds are nullified by this combo.",
	"If the combo is solid and the player pushes it with at least one Key, it changes to the next combo, the 'Lock Blocks' Screen State is set, and one key is used up.",
	"Identical to Lock Block, but if any other Lock Blocks are opened on the same screen, this changes to the next combo.",
	"If the combo is solid and the player pushes it with the Boss Key, it changes to the next combo and the 'Boss Lock Blocks' Screen State is set.",
	"Identical to Lock Block (Boss), but if any other Boss Lock Blocks are opened on the same screen, this changes to the next combo.",
	"If this combo is solid, the Ladder can be used to cross over it. Only works on layer 0.",
	"When touched, this combo produces a Ghini and changes to the next combo in the list.",
	//Chests
	"", "", "", "", "", "",
	"If the player touches this, the Screen States are cleared, and the player is re-warped back into the screen, effectively resetting the screen entirely.",
	"Press the 'Start' button when the player is standing on the bottom of this combo, and the Save menu appears. Best used with the Save Point->Continue Here Screen Flag.",
	"Identical to Save Point, but the Quit option is also available in the menu.",
	"The player marches down into this combo and is warped via Tile Warp B if they step on this. The combo's tile will be drawn above the player during this animation.",
	"The player marches down into this combo and is warped via Tile Warp C if they step on this. The combo's tile will be drawn above the player during this animation.",
	"The player marches down into this combo and is warped via Tile Warp D if they step on this. The combo's tile will be drawn above the player during this animation.",
	"The player is warped via Tile Warp B if they step on the bottom half of this combo.",
	"The player is warped via Tile Warp C if they step on the bottom half of this combo.",
	"The player is warped via Tile Warp D if they step on the bottom half of this combo.",
	"The player is warped via Tile Warp B if they touch any part of this combo, but their on-screen position remains the same. Ground enemies can't enter.",
	"The player is warped via Tile Warp C if they touch any part of this combo, but their on-screen position remains the same. Ground enemies can't enter.",
	"The player is warped via Tile Warp D if they touch any part of this combo, but their on-screen position remains the same. Ground enemies can't enter.",
	"The player marches up into this combo and is warped via Tile Warp B if they step on this. The combo's tile will be drawn above the player during this animation.",
	"The player marches up into this combo and is warped via Tile Warp C if they step on this. The combo's tile will be drawn above the player during this animation.",
	"The player marches up into this combo and is warped via Tile Warp D if they step on this. The combo's tile will be drawn above the player during this animation.",
	"The player is warped via Tile Warp B if they swim on this combo. Otherwise, this is identical to Water.",
	"The player is warped via Tile Warp C if they swim on this combo. Otherwise, this is identical to Water.",
	"The player is warped via Tile Warp D if they swim on this combo. Otherwise, this is identical to Water.",
	"The player is warped via Tile Warp B if they dive on this combo. Otherwise, this is identical to Water.",
	"The player is warped via Tile Warp C if they dive on this combo. Otherwise, this is identical to Water.",
	"The player is warped via Tile Warp D if they dive on this combo. Otherwise, this is identical to Water.",
	"Identical to Stairs [A], but the Tile Warp used (A, B, C, or D) is chosen at random. Use this only in screens where all four Tile Warps are defined.",
	"Identical to Direct Warp [A], but the Tile Warp used (A, B, C, or D) is chosen at random. Use this only in screens where all four Tile Warps are defined.",
	"As soon as this combo appears on the screen, Side Warp A is triggered. This is best used with secret combos or combo cycling.",
	"As soon as this combo appears on the screen, Side Warp B is triggered. This is best used with secret combos or combo cycling.",
	"As soon as this combo appears on the screen, Side Warp C is triggered. This is best used with secret combos or combo cycling.",
	"As soon as this combo appears on the screen, Side Warp D is triggered. This is best used with secret combos or combo cycling.",
	"Identical to Auto Side Warp [A], but the Side Warp used (A, B, C, or D) is chosen at random. Use this only in screens where all four Side Warps are defined.",
	"Identical to Stairs [A], but the player will be warped as soon as they touch the edge of this combo.",
	"Identical to Stairs [B], but the player will be warped as soon as they touch the edge of this combo.",
	"Identical to Stairs [C], but the player will be warped as soon as they touch the edge of this combo.",
	"Identical to Stairs [D], but the player will be warped as soon as they touch the edge of this combo.",
	"Identical to Stairs [Random], but the player will be warped as soon as they touch the edge of this combo.",
	"Identical to Step->Secrets (Temporary), but Screen Secrets are triggered as soon as the player touches the edge of this combo.",
	"Identical to Step->Secrets (Permanent), but Screen Secrets are triggered as soon as the player touches the edge of this combo.",
	"When the player steps on this combo, it will change into the next combo in the list.",
	"Identical to Step->Next, but if other instances of this particular combo are stepped on, this also changes to the next combo in the list.",
	"When the player steps on this combo, each of the Step->Next combos on screen will change to the next combo after them in the list.",
	"When the player steps on a Step->Next (All) type combo, this will change into the next combo in the list.",
	"Enemies cannot enter or appear on this combo.",
	"Level 1 player arrows that hit this combo are destroyed. Enemy arrows are unaffected.",
	"Level 1 or 2 player arrows that hit this combo are destroyed. Enemy arrows are unaffected.",
	"All player arrows that hit this combo are destroyed. Enemy arrows are unaffected.",
	"Level 1 player boomerangs bounce off this combo. Enemy boomerangs are unaffected.",
	"Level 1 or 2 player boomerangs bounce off this combo. Enemy boomerangs are unaffected.",
	"All player boomerangs bounce off this combo. Enemy boomerangs are unaffected.",
	"The player's sword beams or enemy sword beams that hit this combo are destroyed.",
	"All weapons that hit this combo are either destroyed, or bounce off.",
	"Enemy fireballs and reflected fireballs that hit this combo are destroyed.",
	// More damage
	"", "", "",
	"", //Unused
	"A Spinning Tile immediately appears on this combo, using the combo's tile to determine its sprite. The combo then changes to the next in the list.",
	"", // Unused
	"While this combo is on the screen, all action is frozen, except for FFC animation and all scripts. Best used in conjunction with Changer FFCs or scripts.",
	"While this combo is on the screen, FFCs and FFC scripts will be frozen. Best used in conjunction with combo cycling, screen secrets or global scripts.",
	"Enemies that don't fly or jump cannot enter or appear on this combo.",
	"Identical to Slash, but instead of changing into the Under Combo when slashed, this changes to the next combo in the list.",
	"Identical to Slash (Item), but instead of changing into the Under Combo when slashed, this changes to the next combo in the list.",
	"Identical to Bush, but instead of changing into the Under Combo when slashed, this changes to the next combo in the list.",
	// Continuous variation
	"Identical to Slash, but if slashing this combo changes it to another slash-affected combo, then that combo will also change.",
	"Identical to Slash->Item, but if slashing this combo changes it to another slash-affected combo, then that combo will also change.",
	"Identical to Bush, but if slashing this combo changes it to another slash-affected combo, then that combo will also change.",
	"Identical to Flowers, but if slashing this combo changes it to another slash-affected combo, then that combo will also change.",
	"Identical to Tall Grass, but if slashing this combo changes it to another slash-affected combo, then that combo will also change.",
	"Identical to Slash->Next, but if slashing this combo changes it to another slash-affected combo, then that combo will also change.",
	"Identical to Slash->Next (Item), but if slashing this combo changes it to another slash-affected combo, then that combo will also change.",
	"Identical to Bush->Next, but if slashing this combo changes it to another slash-affected combo, then that combo will also change.",
	"Identical to Eyeball (8-Way A), but only the four cardinal directions/sets of tiles are used (up, down, left and right, respectively).",
	"Identical to Tall Grass, but instead of changing into the Under Combo when slashed, this changes to the next combo in the list.",
	// Script types
	"This type has no built-in effect, but can be given special significance with ZASM or ZScript.", //1
	"This type has no built-in effect, but can be given special significance with ZASM or ZScript.",
	"This type has no built-in effect, but can be given special significance with ZASM or ZScript.",
	"This type has no built-in effect, but can be given special significance with ZASM or ZScript.",
	"This type has no built-in effect, but can be given special significance with ZASM or ZScript.", //5
	"This type has no built-in effect, but can be given special significance with ZASM or ZScript.",
	"This type has no built-in effect, but can be given special significance with ZASM or ZScript.",
	"This type has no built-in effect, but can be given special significance with ZASM or ZScript.",
	"This type has no built-in effect, but can be given special significance with ZASM or ZScript.",
	"This type has no built-in effect, but can be given special significance with ZASM or ZScript.", //10
	"This type has no built-in effect, but can be given special significance with ZASM or ZScript.",
	"This type has no built-in effect, but can be given special significance with ZASM or ZScript.",
	"This type has no built-in effect, but can be given special significance with ZASM or ZScript.",
	"This type has no built-in effect, but can be given special significance with ZASM or ZScript.",
	"This type has no built-in effect, but can be given special significance with ZASM or ZScript.", //15
	"This type has no built-in effect, but can be given special significance with ZASM or ZScript.",
	"This type has no built-in effect, but can be given special significance with ZASM or ZScript.",
	"This type has no built-in effect, but can be given special significance with ZASM or ZScript.",
	"This type has no built-in effect, but can be given special significance with ZASM or ZScript.",
	"This type has no built-in effect, but can be given special significance with ZASM or ZScript.", //20
	//Generic
	"Generic combos can be configured to do a wide variety of things based on attributes.",
	"Pitfall combos act as either bottomless pits or warps, including a fall animation.",
	"Step->Effects combos can cause SFX, and also act like a landmine, spawning a weapon.",
	"Bridge combos can be used to block combos under them from having an effect.",
	"",
	"",
	"Switchblock combos change based on switch states toggled by Switch combos. They can also change"
		" the combo at the same position on any layer.",
	"Emits light in a radius in dark rooms (when \"Quest->Options->Misc->New Dark Rooms\" is enabled)"
};

static const char *flag_help_string[mfMAX] =
{
	"",
	"Allows the Player to push the combo up or down once, triggering Screen Secrets (or just the 'Stairs', secret combo) as well as Block->Shutters.",
	"Allows the Player to push the combo in any direction once, triggering Screen Secrets (or just the 'Stairs', secret combo) as well as Block->Shutters.",
	"Triggers Screen Secrets when the Player plays the Whistle on it. Is replaced with the 'Whistle' Secret Combo. Doesn't interfere with Whistle related Screen Flags.",
	"Triggers Screen Secrets when the Player touches it with fire from any source (Candle, Wand, Din's Fire, etc.) Is replaced with the 'Blue Candle' Secret Combo.",
	"Triggers Screen Secrets when the Player touches it with one of his Arrows. Is replaced with the 'Wooden Arrow' Secret Combo.",
	"Triggers Screen Secrets when the middle part of a Bomb explosion touches it. Is replaced with the 'Bomb' Secret Combo.",
	"Makes a heart circle appear on screen when the Player steps on it, and refills his life. See also the Heart Circle-related Quest Rules.",
	"Place in paths to define the path the Player travels when using the Raft. Use with Dock-type combos. If a path branches, the Player takes the clockwise-most path.",
	"When placed on an Armos-type combo, causes the 'Stairs'  Secret Combo to appear when the Armos is triggered, instead of the screen's Under Combo.",
	"When placed on an Armos or treasure chest, causes the room's Special Item to appear when the combo is activated. Requires the 'Special Item' Room Type.",
	"Triggers Screen Secrets when the middle part of a Super Bomb explosion touches it. Is replaced with the 'Super Bomb' Secret Combo.",
	"Place at intersections of Raft flag paths to define points where the player may change directions. Change directions by holding down a directional key.",
	"When the Player dives on a flagged water-type combo they will recieve the screen's Special Item. Requires the 'Special Item' Room Type.",
	"Combos with this flag will flash white when viewed with the Lens of Truth item.",
	"When the Player steps on this flag, the quest will end, and the credits will roll.",
	// 16-31
	"",
	"",
	"",//18
	"",
	"",
	"",//21
	"",
	"",
	"",//24
	"",
	"",
	"",//27
	"",
	"",
	"",//30
	"",
	// Anyway...
	"Creates the lowest-numbered enemy with the 'Spawned by 'Horz Trap' Combo Type/Flag' enemy data flag on the flagged combo.",
	"Creates the lowest-numbered enemy with the 'Spawned by 'Vert Trap' Combo Type/Flag' enemy data flag on the flagged combo.",
	"Creates the lowest-numbered enemy with the 'Spawned by '4-Way Trap' Combo Type/Flag' enemy data flag on the flagged combo.",
	"Creates the lowest-numbered enemy with the 'Spawned by 'LR Trap' Combo Type/Flag' enemy data flag on the flagged combo.",
	"Creates the lowest-numbered enemy with the 'Spawned by 'UD Trap' Combo Type/Flag' enemy data flag on the flagged combo.",
	// Enemy 0-9
	"",
	"",
	"",//2
	"",
	"",
	"",//5
	"",
	"",
	"",//8
	"",
	//Anyway...
	"Allows the Player to push the combo left or right once, triggering Screen Secrets (or just the 'Stairs', secret combo) as well as Block->Shutters.",
	"Allows the Player to push the combo up once, triggering Screen Secrets (or just the 'Stairs', secret combo) as well as Block->Shutters.",
	"Allows the Player to push the combo down once, triggering Screen Secrets (or just the 'Stairs', secret combo) as well as Block->Shutters.",
	"Allows the Player to push the combo left once, triggering Screen Secrets (or just the 'Stairs', secret combo) as well as Block->Shutters.",
	"Allows the Player to push the combo right once, triggering Screen Secrets (or just the 'Stairs', secret combo) as well as Block->Shutters.",
	// Push Silent
	"",//52
	"",
	"",
	"",
	"",
	"",
	"",
	"",//59
	"",
	"",
	"",
	"",
	"",
	"",
	//Anyway...
	"Pushing blocks onto ALL Block Triggers will trigger Screen Secrets (or just the 'Stairs' secret combo) as well as Block->Shutters.",
	"Prevents push blocks from being pushed onto the flagged combo, even if it is not solid.",
	"Triggers Screen Secrets when the Player touches it with one of his Boomerangs. Is replaced with the 'Wooden Boomerang' Secret Combo.",
	"Triggers Screen Secrets when the Player touches it with a level 2 or higher Boomerang. Is replaced with the 'Magic Boomerang' Secret Combo.",
	"Triggers Screen Secrets when the Player touches it with a level 3 or higher Boomerang. Is replaced with the 'Fire Boomerang' Secret Combo.",
	"Triggers Screen Secrets when the Player touches it with a level 2 or higher Arrow. Is replaced with the 'Silver Arrow' Secret Combo.",
	"Triggers Screen Secrets when the Player touches it with a level 3 or higher Arrow. Is replaced with the 'Golden Arrow' Secret Combo.",
	"Triggers Screen Secrets when the Player touches it with fire from a level 2 Candle, a Wand, or Din's Fire. Is replaced with the 'Red Candle' Secret Combo.",
	"Triggers Screen Secrets when the Player touches it with fire from a Wand, or Din's Fire. Is replaced with the 'Wand Fire' Secret Combo.",
	"Triggers Screen Secrets when the Player touches it with Din's Fire. Is replaced with the 'Din's Fire' Secret Combo.",
	"Triggers Screen Secrets when the Player touches it with Wand magic, be it fire or not. Is replaced with the 'Wand Magic' Secret Combo.",
	"Triggers Screen Secrets when the Player touches it with reflected Wand magic. Is replaced with the 'Reflected Magic' Secret Combo.",
	"Triggers Screen Secrets when the Player touches it with a Shield-reflected fireball. Is replaced with the 'Reflected Fireball' Secret Combo.",
	"Triggers Screen Secrets when the Player touches it with one of his Swords. Is replaced with the 'Wooden Sword' Secret Combo.",
	"Triggers Screen Secrets when the Player touches it with a level 2 or higher Sword. Is replaced with the 'White Sword' Secret Combo.",
	"Triggers Screen Secrets when the Player touches it with a level 3 or higher Sword. Is replaced with the 'Magic Sword' Secret Combo.",
	"Triggers Screen Secrets when the Player touches it with a level 4 or higher Sword. Is replaced with the 'Master Sword' Secret Combo.",
	"Triggers Screen Secrets when the Player touches it with one of his Sword beams. Is replaced with the 'Sword Beam' Secret Combo.",
	"Triggers Screen Secrets when the Player touches it with a level 2 or higher Sword's beam. Is replaced with the 'White Sword Beam' Secret Combo.",
	"Triggers Screen Secrets when the Player touches it with a level 3 or higher Sword's beam. Is replaced with the 'Magic Sword Beam' Secret Combo.",
	"Triggers Screen Secrets when the Player touches it with a level 4 or higher Sword's beam. Is replaced with the 'Master Sword Beam' Secret Combo.",
	"Triggers Screen Secrets when the Player touches it with one of his Hookshot hooks. Is replaced with the 'Hookshot' Secret Combo.",
	"Triggers Screen Secrets when the Player touches it with one of his Wands. Is replaced with the 'Wand' Secret Combo.",
	"Triggers Screen Secrets when the Player pounds it with one of his Hammers. Is replaced with the 'Hammer' Secret Combo.",
	"Triggers Screen Secrets when the Player touches it with any weapon or projectile. Is replaced with the 'Any Weapon' Secret Combo.",
	"A push block pushed onto this flag will cycle to the next combo in the list, and lose the Push flag that was presumably on it.",
	"Makes a heart circle appear on screen when the Player steps on it, and refills his magic. See also the Heart Circle-related Quest Rules.",
	"Makes a heart circle appear on screen when the Player steps on it, and refills his life and magic. See also the Heart Circle-related Quest Rules.",
	"When stacked with a Trigger Combo Flag, it prevents the triggered Secrets process from changing all other flagged combos on-screen.",
	"Similar to 'Trigger->Self Only', but the Secret Tile (16-31) flagged combos will still change. (The 'Hit All Triggers->16-31' Screen Flag overrides this.)",
	"Enemies cannot enter or appear on the flagged combo.",
	"Enemies that don't fly or jump cannot enter or appear on the flagged combo.",
	//Script Flags follow.
	"",
	"",
	"",
	"",
	"",
	//Raft bounce flag! ^_^
	"When the Player is rafting, and hits this flag, they will be turned around."
};
//}


std::string getTypeHelpText(int32_t id)
{
	std::string typehelp;
	switch(id)
	{
		case cNONE:
			typehelp = "Select a Type, then click this button to find out what it does.";
			break;
		case cDAMAGE1: case cDAMAGE2: case cDAMAGE3: case cDAMAGE4:
		case cDAMAGE5: case cDAMAGE6: case cDAMAGE7:
		{
			char buf[512];
			int32_t lvl = (id < cDAMAGE5 ? (id - cDAMAGE1 + 1) : (id - cDAMAGE5 + 1));
			int32_t d = -combo_class_buf[id].modify_hp_amount/8;
			char buf2[80];
			if(d==1)
				sprintf(buf2,"1/2 of a heart.");
			else
				sprintf(buf2,"%d heart%s.", d/2, d == 2 ? "" : "s");
			sprintf(buf,"If the Player touches this combo without Boots that protect against Damage Combo Level %d, they are damaged for %s.",lvl,buf2);
			typehelp = buf;
			break;
		}
		
		case cCHEST: case cLOCKEDCHEST: case cBOSSCHEST:
			typehelp = "If no button is assigned, the chest opens when pushed against from a valid side. If buttons are assigned,"
				" then when the button is pressed while facing the chest from a valid side.\n"
				"When the chest is opened, if it has the 'Armos/Chest->Item' combo flag, the player will recieve the item set in the screen's catchall value, and the combo will advance to the next combo.";
			if(id==cLOCKEDCHEST)
				typehelp += "\nRequires a key to open.";
			else if(id==cBOSSCHEST)
				typehelp += "\nRequires the Boss Key to open.";
			break;
		case cSIGNPOST:
			typehelp = "Signpost combos can be set to display a string. This can be hard-coded,"
				" or variable. The message will display either on button press of a"
				" set button or walking into the sign if no button is set.";
			break;
		case cCHEST2:
			typehelp = "Acts as a chest that can't be opened. Becomes 'opened' (advancing to the next combo) when any 'Treasure Chest (Normal)' is opened on the screen.";
			break;
		case cLOCKEDCHEST2:
			typehelp = "Acts as a chest that can't be opened. Becomes 'opened' (advancing to the next combo) when any 'Treasure Chest (Locked)' is opened on the screen.";
			break;
		case cBOSSCHEST2:
			typehelp = "Acts as a chest that can't be opened. Becomes 'opened' (advancing to the next combo) when any 'Treasure Chest (Boss)' is opened on the screen.";
			break;
		
		case cHSBRIDGE: case cZELDA: case cUNDEF: case cCHANGE: case cSPINTILE2:
			typehelp = "Unimplemented type, do not use!";
			break;
		
		case cSPOTLIGHT:
			typehelp = "Shoots a beam of light which reflects off of mirrors, and can trigger light triggers.";
			break;
		
		case cGLASS:
			typehelp = "Does not block light beams, even if solid";
			break;
		
		case cLIGHTTARGET:
			typehelp = "If all targets onscreen are lit by light beams, secrets will be triggered.";
			break;
		case cCSWITCH:
			typehelp = "Switch combos, when triggered (Triggers tab), toggle a switch state for the current 'level'."
				" These states affect Switchblock combos in any dmaps of the same level, and are saved between sessions.";
			break;
		case cSWITCHHOOK:
			typehelp = "When hit with a switch-hook, swaps position with the player (staying on the same layer).";
			break;
		default:
			if(combotype_help_string[id] && combotype_help_string[id][0])
				typehelp = combotype_help_string[id];
			else typehelp = "?? Missing documentation! ??";
			break;
	}
	return typehelp;
}
std::string getFlagHelpText(int32_t id)
{
	std::string flaghelp = "?? Missing documentation! ??";
	if(flag_help_string[id] && flag_help_string[id][0])
		flaghelp = flag_help_string[id];
	switch(id)
	{
		case 0:
			flaghelp = "Select a Flag, then click this button to find out what it does.";
			break;
		case mfSECRETS01: case mfSECRETS02: case mfSECRETS03: case mfSECRETS04:
		case mfSECRETS05: case mfSECRETS06: case mfSECRETS07: case mfSECRETS08:
		case mfSECRETS09: case mfSECRETS10: case mfSECRETS11: case mfSECRETS12:
		case mfSECRETS13: case mfSECRETS14: case mfSECRETS15: case mfSECRETS16:
			flaghelp = "When Screen Secrets are triggered, this is replaced with Secret Combo "
				+ std::to_string(id) + ". (Also, flagged Destructible Combos"
				" will use that Secret Combo instead of the Under Combo.)";
			break;
		case mfENEMY0: case mfENEMY1: case mfENEMY2: case mfENEMY3: case mfENEMY4:
		case mfENEMY5: case mfENEMY6: case mfENEMY7: case mfENEMY8: case mfENEMY9:
		{
			int32_t enemynum = id - 37 + 1;
			flaghelp = "When the "+string(ordinal(enemynum))+" enemy in the Enemy List is spawned,"
				" it appears on this flag instead of using the Enemy Pattern."
				" The uppermost, then leftmost, instance of this flag is used.";
			break;
		}
		case mfPUSHUDNS: case mfPUSHLRNS: case mfPUSH4NS: case mfPUSHUNS:
		case mfPUSHDNS: case mfPUSHLNS: case mfPUSHRNS: case mfPUSHUDINS:
		case mfPUSHLRINS: case mfPUSH4INS: case mfPUSHUINS: case mfPUSHDINS:
		case mfPUSHLINS: case mfPUSHRINS:
		{
			int32_t t = ((id-mfPUSHUDNS) % 7);
			flaghelp = "Allows the Player to push the combo "
				+ std::string((t == 0) ? "up and down" : (t == 1) ? "left and right" : (t == 2) ? "in any direction" : (t == 3) ? "up" : (t == 4) ? "down" : (t == 5) ? "left" : "right")
				+ std::string((id>=mfPUSHUDINS) ? "many times":"once")
				+ " triggering Block->Shutters but not Screen Secrets.";
			break;
		}
		case mfPUSHED:
		{
			flaghelp = "This flag is placed on Push blocks with the 'Once' property after they have been pushed.";
			break;
		}
		case mfSIDEVIEWLADDER:
		{
			flaghelp = "On a sideview screen, allows climbing. The topmost ladder in a column doubles as a Sideview Platform.";
			break;
		}
		case mfSIDEVIEWPLATFORM:
		{
			flaghelp = "On a sideview screen, can be stood on top of, even when nonsolid. Can be jumped through"
				" from below, and depending on QRs, can also be dropped through.";
			break;
		}
		case mfNOENEMYSPAWN:
		{
			flaghelp = "No enemies will spawn on this flag.";
			break;
		}
		case mfENEMYALL:
		{
			flaghelp = "All enemies will spawn on this flag instead of using the spawn pattern.";
			break;
		}
		case mfSECRETSNEXT:
		{
			flaghelp = "When secrets are triggered, the combo in this position becomes the next"
				" combo in the list.";
			break;
		}
		case mfSCRIPT1: case mfSCRIPT2: case mfSCRIPT3: case mfSCRIPT4: case mfSCRIPT5:
		case mfSCRIPT6: case mfSCRIPT7: case mfSCRIPT8: case mfSCRIPT9: case mfSCRIPT10:
		case mfSCRIPT11: case mfSCRIPT12: case mfSCRIPT13: case mfSCRIPT14: case mfSCRIPT15:
		case mfSCRIPT16: case mfSCRIPT17: case mfSCRIPT18: case mfSCRIPT19: case mfSCRIPT20:
		case mfPITHOLE: case mfPITFALLFLOOR: case mfLAVA: case mfICE: case mfICEDAMAGE:
		case mfDAMAGE1: case mfDAMAGE2: case mfDAMAGE4: case mfDAMAGE8: case mfDAMAGE16:
		case mfDAMAGE32: case mfTROWEL: case mfTROWELNEXT: case mfTROWELSPECIALITEM:
		case mfSLASHPOT: case mfLIFTPOT: case mfLIFTORSLASH: case mfLIFTROCK:
		case mfLIFTROCKHEAVY: case mfDROPITEM: case mfSPECIALITEM: case mfDROPKEY:
		case mfDROPLKEY: case mfDROPCOMPASS: case mfDROPMAP: case mfDROPBOSSKEY:
		case mfSPAWNNPC: case mfSWITCHHOOK:
		{
			flaghelp = "These flags have no built-in effect,"
				" but can be given special significance with ZASM or ZScript.";
			break;
		}
		case mfFREEZEALL: case mfFREZEALLANSFFCS: case mfFREEZEFFCSOLY: case mfSCRITPTW1TRIG:
		case mfSCRITPTW2TRIG: case mfSCRITPTW3TRIG: case mfSCRITPTW4TRIG: case mfSCRITPTW5TRIG:
		case mfSCRITPTW6TRIG: case mfSCRITPTW7TRIG: case mfSCRITPTW8TRIG: case mfSCRITPTW9TRIG:
		case mfSCRITPTW10TRIG:
		{
			flaghelp = "Not yet implemented";
			break;
		}
		case mfNOMIRROR:
		{
			flaghelp = "While touching this flag, attempting to use a Mirror item will fail.";
			break;
		}
	}
	return flaghelp;
}
void ctype_help(int32_t id)
{
	InfoDialog(moduledata.combo_type_names[id],getTypeHelpText(id)).show();
}
void cflag_help(int32_t id)
{
	InfoDialog(moduledata.combo_flag_names[id],getFlagHelpText(id)).show();
}
//Load all the info for the combo type and checked flags
void ComboEditorDialog::loadComboType()
{
	static std::string dirstr[] = {"up","down","left","right"};
	if(lasttype != local_comboref.type) //Load type helpinfo
	{
		lasttype = local_comboref.type;
		typehelp = getTypeHelpText(lasttype);
	}
	string l_flag[16];
	string l_attribyte[8];
	string l_attrishort[8];
	string l_attribute[4];
	#define FL(fl) (local_comboref.usrflags & (fl))
	for(size_t q = 0; q < 16; ++q)
	{
		l_flag[q] = "Flags["+to_string(q)+"]";
		h_flag[q].clear();
		if(q > 7) continue;
		l_attribyte[q] = "Attribytes["+to_string(q)+"]:";
		l_attrishort[q] = "Attrishorts["+to_string(q)+"]:";
		h_attribyte[q].clear();
		h_attrishort[q].clear();
		if(q > 3) continue;
		l_attribute[q] = "Attributes["+to_string(q)+"]:";
		h_attribute[q].clear();
	}
	switch(lasttype) //Label names
	{
		case cSTAIR: case cSTAIRB: case cSTAIRC: case cSTAIRD: case cSTAIRR:
		case cSWIMWARP: case cSWIMWARPB: case cSWIMWARPC: case cSWIMWARPD:
		case cDIVEWARP: case cDIVEWARPB: case cDIVEWARPC: case cDIVEWARPD:
		case cPIT: case cPITB: case cPITC: case cPITD: case cPITR:
		case cAWARPA: case cAWARPB: case cAWARPC: case cAWARPD: case cAWARPR:
		case cSWARPA: case cSWARPB: case cSWARPC: case cSWARPD: case cSWARPR:
		{
			l_attribyte[0] = "Sound:";
			h_attribyte[0] = "SFX to play during the warp";
			break;
		}
		case cTRIGNOFLAG: case cSTRIGNOFLAG:
		case cTRIGFLAG: case cSTRIGFLAG:
		{
			l_attribyte[0] = "Sound:";
			h_attribyte[0] = "SFX to play when triggered";
			break;
		}
		case cSTEP: case cSTEPSAME: case cSTEPALL:
		{
			l_flag[0] = "Heavy";
			h_flag[0] = "Requires Heavy Boots to trigger";
			l_attribyte[0] = "Sound:";
			h_attribyte[0] = "SFX to play when triggered";
			l_attribyte[1] = "Req. Item";
			h_attribyte[1] = "Item ID that must be owned in order to trigger. If '0', no item is required.";
			break;
		}
		case cSTEPCOPY:
		{
			l_flag[0] = "Heavy";
			h_flag[0] = "Requires Heavy Boots to trigger";
			break;
		}
		case cWATER:
		{
			l_flag[0] = "Is Lava";
			h_flag[0] = "If a liquid is Lava, it uses a different drowning sprite, and only flippers with the"
				" 'Can Swim In Lava' flag set will apply.";
			l_flag[1] = "Modify HP (Passive)";
			h_flag[1] = "If checked, the player's HP will change over time while in the liquid"
				" (either healing or damaging).";
			l_flag[2] = "Solid is Land";
			h_flag[2] = "Solid areas of the combo are treated as non-solid land";
			l_flag[3] = "Solid is Shallow Liquid";
			h_flag[3] = "Solid areas of the combo are treated as non-solid Shallow Liquid combo";
			l_attribute[0] = "Drown Damage:";
			h_attribute[0] = "The amount of damage dealt when drowning, in HP points. If negative, drowning will heal the player.";
			l_attribyte[0] = "Flipper Level:";
			h_attribyte[0] = "The minimum level flippers required to swim in the water. Flippers of lower level will have no effect.";
			if(FL(cflag2)) //Modify HP
			{
				l_flag[4] = "Rings affect HP Mod";
				h_flag[4] = "Ring items defense reduces damage from HP Mod";
				l_flag[5] = "Mod SFX only on HP change";
				h_flag[5] = "Only play the HP Mod SFX when HP actually changes";
				l_flag[6] = "Damage causes hit anim";
				h_flag[6] = "HP Mod Damage triggers the hit animation and invincibility frames";
				l_attribute[1] = "HP Modification:";
				h_attribute[1] = "How much HP should be modified by (negative for damage)";
				l_attribute[2] = "HP Mod SFX:";
				h_attribute[2] = "What SFX should play when HP is modified";
				l_attribyte[1] = "HP Delay:";
				h_attribyte[1] = "The number of frames between HP modifications";
				l_attribyte[2] = "Req Itemclass:";
				h_attribyte[2] = "If non-zero, an itemclass number which, if owned, will prevent HP modification.";
				l_attribyte[3] = "Req Itemlevel:";
				h_attribyte[3] = "A minimum item level to go with 'Req Itemclass'.";
			}
			break;
		}
		case cSHALLOWWATER:
		{
			l_flag[1] = "Modify HP (Passive)";
			h_flag[1] = "If checked, the player's HP will change over time while in the liquid"
				" (either healing or damaging).";
			l_attribyte[0] = "Sound";
			h_attribyte[0] = "SFX ID to play when stepping in the shallow liquid";
			if(FL(cflag2)) //Modify HP
			{
				l_attribute[1] = "HP Modification:";
				h_attribute[1] = "How much HP should be modified by (negative for damage)";
				l_attribute[2] = "HP Mod SFX:";
				h_attribute[2] = "What SFX should play when HP is modified";
				l_attribyte[1] = "HP Delay:";
				h_attribyte[1] = "The number of frames between HP modifications";
				l_attribyte[2] = "Req Itemclass:";
				h_attribyte[2] = "If non-zero, an itemclass number which, if owned, will prevent HP modification.";
				l_attribyte[3] = "Req Itemlevel:";
				h_attribyte[3] = "A minimum item level to go with 'Req Itemclass'.";
			}
			break;
		}
		case cARMOS:
		{
			l_flag[0] = "Specify";
			h_flag[0] = "If checked, attribytes are used to specify enemy IDs. Otherwise, the lowest"
				" enemy ID with the armos flag checked will be spawned.";
			if(FL(cflag1))
			{
				l_flag[1] = "Random";
				h_flag[1] = "Randomly choose between two enemy IDs (50/50)";
				if(FL(cflag2))
				{
					l_attribyte[0] = "Enemy 1:";
					h_attribyte[0] = "The first enemy ID, 50% chance of being spawned";
					l_attribyte[1] = "Enemy 2:";
					h_attribyte[1] = "The second enemy ID, 50% chance of being spawned";
				}
				else
				{
					l_attribyte[0] = "Enemy:";
					h_attribyte[0] = "The enemy ID to be spawned";
				}
			}
			break;
		}
		case cCVUP: case cCVDOWN: case cCVLEFT: case cCVRIGHT:
		{
			l_flag[1] = "Custom Speed";
			h_flag[1] = "Uses a custom speed/direction via attributes. If disabled, moves at 2 pixels every 3 frames in the " + dirstr[lasttype-cCVUP] + "ward direction.";
			if(FL(cflag2)) //Custom speed
			{
				l_attribute[0] = "X Speed:";
				h_attribute[0] = "Pixels moved in the X direction per rate frames";
				l_attribute[1] = "Y Speed:";
				h_attribute[1] = "Pixels moved in the Y direction per rate frames";
				l_attribyte[0] = "Rate:";
				h_attribyte[0] = "Every this many frames the conveyor moves by the set speeds. If set to 0, acts as if set to 1.";
			}
			break;
		}
		case cTALLGRASS:
		{
			l_flag[0] = "Decoration Sprite";
			h_flag[0] = "Spawn a decoration when slashed";
			if(FL(cflag1))
			{
				l_flag[9] = "Use Clippings Sprite";
				h_flag[9] = "Use a system clipping sprite instead of a Sprite Data sprite";
				if(FL(cflag10))
				{
					l_attribyte[0] = "Clipping Sprite:";
					h_attribyte[0] = "0 and 1 = Bush Leaves, 2 = Flowers, 3 = Grass";
				}
				else
				{
					l_attribyte[0] = "Sprite:";
					h_attribyte[0] = "Sprite Data sprite ID to display as a clipping";
				}
			}
			l_flag[1] = "Set Dropset";
			h_flag[1] = "Allows specifying the dropset to use as an attribyte";
			l_flag[2] = "Custom Slash SFX";
			h_flag[2] = "Specify a custom slash SFX";
			l_attribyte[3] = "Walking Sound:";
			h_attribyte[3] = "The SFX to play when the player walks through this combo. If 0, no sound is played.";
			if(FL(cflag2))
			{
				l_flag[10] = "Specific Item";
				h_flag[10] = "Drop a specific item instead of an item from a dropset";
				if(FL(cflag11))
				{
					l_attribyte[1] = "Item:";
					h_attribyte[1] = "The item ID to drop";
				}
				else
				{
					l_attribyte[1] = "Dropset:";
					h_attribyte[1] = "The dropset to select a drop item from";
				}
			}
			if(FL(cflag3))
			{
				l_attribyte[2] = "Slash Sound:";
				h_attribyte[2] = "The SFX to play when slashed";
			}
			break;
		}
		case cBUSH: case cBUSHTOUCHY: case cFLOWERS: case cSLASHNEXTTOUCHY:
		{
			l_flag[0] = "Decoration Sprite";
			h_flag[0] = "Spawn a decoration when slashed";
			if(FL(cflag1))
			{
				l_flag[9] = "Use Clippings Sprite";
				h_flag[9] = "Use a system clipping sprite instead of a Sprite Data sprite";
				if(FL(cflag10))
				{
					l_attribyte[0] = "Clipping Sprite:";
					h_attribyte[0] = "0 and 1 = Bush Leaves, 2 = Flowers, 3 = Grass";
				}
				else
				{
					l_attribyte[0] = "Sprite:";
					h_attribyte[0] = "Sprite Data sprite ID to display as a clipping";
				}
			}
			l_flag[1] = "Set Dropset";
			h_flag[1] = "Allows specifying the dropset to use as an attribyte";
			l_flag[2] = "Custom SFX";
			h_flag[2] = "Specify a custom slash SFX";
			if(FL(cflag2))
			{
				l_flag[10] = "Specific Item";
				h_flag[10] = "Drop a specific item instead of an item from a dropset";
				if(FL(cflag11))
				{
					l_attribyte[1] = "Item:";
					h_attribyte[1] = "The item ID to drop";
				}
				else
				{
					l_attribyte[1] = "Dropset:";
					h_attribyte[1] = "The dropset to select a drop item from";
				}
			}
			if(FL(cflag3))
			{
				l_attribyte[2] = "Slash Sound:";
				h_attribyte[2] = "The SFX to play when slashed";
			}
			break;
		}
		case cSLASHITEM:
		{
			l_flag[0] = "Decoration Sprite";
			h_flag[0] = "Spawn a decoration when slashed";
			if(FL(cflag1))
			{
				l_flag[9] = "Use Clippings Sprite";
				h_flag[9] = "Use a system clipping sprite instead of a Sprite Data sprite";
				if(FL(cflag10))
				{
					l_attribyte[0] = "Clipping Sprite:";
					h_attribyte[0] = "0 and 1 = Bush Leaves, 2 = Flowers, 3 = Grass";
				}
				else
				{
					l_attribyte[0] = "Sprite:";
					h_attribyte[0] = "Sprite Data sprite ID to display as a clipping";
				}
			}
			l_flag[1] = "Set Dropset";
			h_flag[1] = "Allows specifying the dropset to use as an attribyte";
			l_flag[2] = "Custom SFX";
			h_flag[2] = "Specify a custom slash SFX";
			if(FL(cflag2))
			{
				l_flag[10] = "Specific Item";
				h_flag[10] = "Drop a specific item instead of an item from a dropset";
				if(FL(cflag11))
				{
					l_attribyte[1] = "Item:";
					h_attribyte[1] = "The item ID to drop";
				}
				else
				{
					l_attribyte[1] = "Dropset:";
					h_attribyte[1] = "The dropset to select a drop item from";
				}
			}
			if(FL(cflag3))
			{
				l_attribyte[2] = "Slash Sound:";
				h_attribyte[2] = "The SFX to play when slashed";
			}
			break;
		}
		case cDAMAGE1: case cDAMAGE2: case cDAMAGE3: case cDAMAGE4:
		case cDAMAGE5: case cDAMAGE6: case cDAMAGE7:
		{
			l_flag[0] = "Custom Damage";
			h_flag[0] = "Uses custom damage amount";
			l_flag[1] = "No Knockback";
			h_flag[1] = "Does not knock the player back when damaging them if checked. Otherwise, knocks the player in the direction"
				" opposite the one they face.";
			if(FL(cflag1))
			{
				l_attribute[0] = "Damage:";
				h_attribute[0] = "The amount of damage, in HP, to deal. Negative amounts heal."
					"\nFor healing, the lowest healing amount combo you are standing on takes effect."
					"\nFor damage, the greatest amount takes priority unless 'Quest->Options->Combos->Lesser Damage Combos Take Priority' is checked.";
			}
			break;
		}
		case cLOCKBLOCK:
		{
			l_flag[0] = "Require Item";
			h_flag[0] = "Require an item in your inventory to unlock the block";
			if(FL(cflag1))
			{
				l_flag[4] = "Eat Item";
				h_flag[4] = "Consume the required item instead of simply requiring its presence";
				if(FL(cflag5))
				{
					l_attribyte[0] = "Consumed Item";
					h_attribyte[0] = "The Item ID required to open the lock block. Consumed.";
				}
				else
				{
					l_attribyte[0] = "Held Item";
					h_attribyte[0] = "The Item ID required to open the lock block. Not consumed.";
				}
				l_flag[1] = "Only Item";
				h_flag[1] = "Only the required item can open this block";
			}
			else
			{
				l_attribute[0] = "Amount:";
				if(FL(cflag4))
					h_attribute[0] = "The amount of the arbitrary counter required to open this block";
				else
					h_attribute[0] = "The amount of keys required to open this block";
			}
			l_flag[3] = "Counter";
			h_flag[3] = "If checked, uses an arbitrary counter instead of keys";
			if(FL(cflag4))
			{
				l_attribyte[1] = "Counter:";
				h_attribyte[1] = "The counter to use to open this block";
				l_flag[7] = "No Drain";
				h_flag[7] = "Requires the counter have the amount, but do not consume from it";
				if(!FL(cflag8))
				{
					l_flag[5] = "Thief";
					h_flag[5] = "Consumes from counter even if you don't have enough";
				}
			}
			
			l_flag[2] = "Custom Unlock Sound";
			h_flag[2] = "Play a custom sound when unlocked";
			if(FL(cflag3))
			{
				l_attribyte[3] = "Unlock Sound:";
				h_attribyte[3] = "The sound to play when unlocking the block";
			}
			break;
		}
		case cBOSSLOCKBLOCK:
		{
			l_flag[2] = "Custom Unlock Sound";
			h_flag[2] = "Play a custom sound when unlocked";
			if(FL(cflag3))
			{
				l_attribyte[3] = "Unlock Sound:";
				h_attribyte[3] = "The sound to play when unlocking the block";
			}
			break;
		}
		case cCHEST: case cLOCKEDCHEST: case cBOSSCHEST:
		{
			l_flag[8] = "Can't use from top";
			h_flag[8] = "Cannot be activated standing to the top side if checked";
			l_flag[9] = "Can't use from bottom";
			h_flag[9] = "Cannot be activated standing to the bottom side if checked";
			l_flag[10] = "Can't use from left";
			h_flag[10] = "Cannot be activated standing to the left side if checked";
			l_flag[11] = "Can't use from right";
			h_flag[11] = "Cannot be activated standing to the right side if checked";
			l_attribyte[2] = "Button:";
			h_attribyte[2] = "Sum all the buttons you want to be usable:\n(A=1, B=2, L=4, R=8, Ex1=16, Ex2=32, Ex3=64, Ex4=128)\n"
				"If no buttons are selected, walking into the chest will trigger it.";
			break;
		}
		case cSIGNPOST:
		{
			l_flag[8] = "Can't use from top";
			h_flag[8] = "Cannot be activated standing to the top side if checked";
			l_flag[9] = "Can't use from bottom";
			h_flag[9] = "Cannot be activated standing to the bottom side if checked";
			l_flag[10] = "Can't use from left";
			h_flag[10] = "Cannot be activated standing to the left side if checked";
			l_flag[11] = "Can't use from right";
			h_flag[11] = "Cannot be activated standing to the right side if checked";
			l_attribyte[2] = "Button:";
			h_attribyte[2] = "Sum all the buttons you want to be usable:\n(A=1, B=2, L=4, R=8, Ex1=16, Ex2=32, Ex3=64, Ex4=128)\n"
				"If no buttons are selected, walking into the signpost will trigger it.";
			l_attribute[0] = "String:";
			h_attribute[0] = "1+: Use specified string\n"
				"-1: Use screen string\n"
				"-2: Use screen catchall as string\n"
				"-10 to -17: Use Screen->D[0] to [7] as string";
			break;
		}
		case cTALLGRASSTOUCHY: case cTALLGRASSNEXT:
		{
			l_flag[0] = "Decoration Sprite";
			h_flag[0] = "Spawn a decoration when slashed";
			if(FL(cflag1))
			{
				l_flag[9] = "Use Clippings Sprite";
				h_flag[9] = "Use a system clipping sprite instead of a Sprite Data sprite";
				if(FL(cflag10))
				{
					l_attribyte[0] = "Clipping Sprite:";
					h_attribyte[0] = "0 and 1 = Bush Leaves, 2 = Flowers, 3 = Grass";
				}
				else
				{
					l_attribyte[0] = "Sprite:";
					h_attribyte[0] = "Sprite Data sprite ID to display as a clipping";
				}
			}
			l_flag[1] = "Set Dropset";
			h_flag[1] = "Allows specifying the dropset to use as an attribyte";
			l_flag[2] = "Custom SFX";
			h_flag[2] = "Specify a custom slash SFX";
			l_attribyte[3] = "Walking Sound:";
			h_attribyte[3] = "The SFX to play when the player walks through this combo. If 0, no sound is played.";
			if(FL(cflag2))
			{
				l_flag[10] = "Specific Item";
				h_flag[10] = "Drop a specific item instead of an item from a dropset";
				if(FL(cflag11))
				{
					l_attribyte[1] = "Item:";
					h_attribyte[1] = "The item ID to drop";
				}
				else
				{
					l_attribyte[1] = "Dropset:";
					h_attribyte[1] = "The dropset to select a drop item from";
				}
			}
			if(FL(cflag3))
			{
				l_attribyte[2] = "Slash Sound:";
				h_attribyte[2] = "The SFX to play when slashed";
			}
			break;
		}
		case cSLASHNEXTITEM: case cBUSHNEXT: case cSLASHITEMTOUCHY:
		case cFLOWERSTOUCHY: case cBUSHNEXTTOUCHY:
		{
			l_flag[0] = "Decoration Sprite";
			h_flag[0] = "Spawn a decoration when slashed";
			if(FL(cflag1))
			{
				l_flag[9] = "Use Clippings Sprite";
				h_flag[9] = "Use a system clipping sprite instead of a Sprite Data sprite";
				if(FL(cflag10))
				{
					l_attribyte[0] = "Clipping Sprite:";
					h_attribyte[0] = "0 and 1 = Bush Leaves, 2 = Flowers, 3 = Grass";
				}
				else
				{
					l_attribyte[0] = "Sprite:";
					h_attribyte[0] = "Sprite Data sprite ID to display as a clipping";
				}
			}
			l_flag[1] = "Set Dropset";
			h_flag[1] = "Allows specifying the dropset to use as an attribyte";
			l_flag[2] = "Custom SFX";
			h_flag[2] = "Specify a custom slash SFX";
			if(FL(cflag2))
			{
				l_flag[10] = "Specific Item";
				h_flag[10] = "Drop a specific item instead of an item from a dropset";
				if(FL(cflag11))
				{
					l_attribyte[1] = "Item:";
					h_attribyte[1] = "The item ID to drop";
				}
				else
				{
					l_attribyte[1] = "Dropset:";
					h_attribyte[1] = "The dropset to select a drop item from";
				}
			}
			if(FL(cflag3))
			{
				l_attribyte[2] = "Slash Sound:";
				h_attribyte[2] = "The SFX to play when slashed";
			}
			break;
		}
		case cSLASHNEXT:
		{
			l_flag[0] = "Decoration Sprite";
			h_flag[0] = "Spawn a decoration when slashed";
			if(FL(cflag1))
			{
				l_flag[9] = "Use Clippings Sprite";
				h_flag[9] = "Use a system clipping sprite instead of a Sprite Data sprite";
				if(FL(cflag10))
				{
					l_attribyte[0] = "Clipping Sprite:";
					h_attribyte[0] = "0 and 1 = Bush Leaves, 2 = Flowers, 3 = Grass";
				}
				else
				{
					l_attribyte[0] = "Sprite:";
					h_attribyte[0] = "Sprite Data sprite ID to display as a clipping";
				}
			}
			l_flag[2] = "Custom SFX";
			h_flag[2] = "Specify a custom slash SFX";
			if(FL(cflag3))
			{
				l_attribyte[2] = "Slash Sound:";
				h_attribyte[2] = "The SFX to play when slashed";
			}
			break;
		}
		case cSLASHNEXTITEMTOUCHY:
		{
			l_flag[0] = "Decoration Sprite";
			h_flag[0] = "Spawn a decoration when slashed";
			if(FL(cflag1))
			{
				l_flag[9] = "Use Clippings Sprite";
				h_flag[9] = "Use a system clipping sprite instead of a Sprite Data sprite";
				if(FL(cflag10))
				{
					l_attribyte[0] = "Clipping Sprite:";
					h_attribyte[0] = "0 and 1 = Bush Leaves, 2 = Flowers, 3 = Grass";
				}
				else
				{
					l_attribyte[0] = "Sprite:";
					h_attribyte[0] = "Sprite Data sprite ID to display as a clipping";
				}
			}
			l_flag[1] = "Set Dropset";
			h_flag[1] = "Allows specifying the dropset to use as an attribyte";
			l_flag[2] = "Custom SFX";
			h_flag[2] = "Specify a custom slash SFX";
			if(FL(cflag2))
			{
				l_flag[10] = "Specific Item";
				h_flag[10] = "Drop a specific item instead of an item from a dropset";
				if(FL(cflag11))
				{
					l_attribyte[1] = "Item:";
					h_attribyte[1] = "The item ID to drop";
				}
				else
				{
					l_attribyte[1] = "Dropset:";
					h_attribyte[1] = "The dropset to select a drop item from";
				}
			}
			if(FL(cflag3))
			{
				l_attribyte[2] = "Slash Sound:";
				h_attribyte[2] = "The SFX to play when slashed";
			}
			break;
		}
		case cSCRIPT1: case cSCRIPT2: case cSCRIPT3: case cSCRIPT4: case cSCRIPT5:
		case cSCRIPT6: case cSCRIPT7: case cSCRIPT8: case cSCRIPT9: case cSCRIPT10:
		case cSCRIPT11: case cSCRIPT12: case cSCRIPT13: case cSCRIPT14: case cSCRIPT15:
		case cSCRIPT16: case cSCRIPT17: case cSCRIPT18: case cSCRIPT19: case cSCRIPT20:
		{
			l_flag[8] = "Generic";
			h_flag[8] = "Attributes/flags act like the Generic combo type.";
			if(!(FL(cflag9))) //Generic flag not set
				break;
		}
		[[fallthrough]];
		case cTRIGGERGENERIC:
		{
			l_flag[0] = "Decoration Sprite";
			h_flag[0] = "Spawn a decoration when triggered";
			if(FL(cflag1))
			{
				l_flag[9] = "Use Clippings Sprite";
				h_flag[9] = "Use a system clipping sprite instead of a Sprite Data sprite";
				if(FL(cflag10))
				{
					l_attribyte[0] = "Clipping Sprite:";
					h_attribyte[0] = "0 and 1 = Bush Leaves, 2 = Flowers, 3 = Grass";
				}
				else
				{
					l_attribyte[0] = "Sprite:";
					h_attribyte[0] = "Sprite Data sprite ID to display as a clipping";
				}
			}
			l_flag[1] = "Drop Item";
			h_flag[1] = "Drop an item when triggered";
			if(FL(cflag2)) //Drop item
			{
				l_flag[10] = "Specific Item";
				h_flag[10] = "Drop a specific item instead of an item from a dropset";
				if(FL(cflag11))
				{
					l_attribyte[1] = "Item:";
					h_attribyte[1] = "The item ID to drop";
				}
				else
				{
					l_attribyte[1] = "Dropset:";
					h_attribyte[1] = "The dropset to select a drop item from";
				}
			}
			l_flag[3] = "Change Combo";
			h_flag[3] = "Become the next combo in the combo list when triggered";
			if(FL(cflag4))
			{
				l_flag[11] = "Undercombo";
				h_flag[11] = "Becomes the screen undercombo instead of next combo in the list";
				if(!FL(cflag12))
				{
					l_flag[4] = "Continuous Trigger";
					h_flag[4] = "When changing to a new combo, if the new combo is a generic combo with continuous flag set, skip it";
				}
			}
			l_flag[7] = "Kill Wpn";
			h_flag[7] = "Destroy the weapon that triggers this combo";
			l_flag[13] = "Drop Enemy";
			h_flag[13] = "Spawn an Enemy when triggered";
			if(FL(cflag14))
			{
				l_attribyte[4] = "Enemy ID";
				h_attribyte[4] = "The Enemy ID to spawn";
			}
			
			l_flag[6] = "Trigger Singular Secret";
			h_flag[6] = "Triggers a single secret flag temporarily";
			if(FL(cflag7|cflag4))
			{
				l_attribyte[2] = "SFX:";
				switch(FL(cflag7|cflag4))
				{
					case cflag4:
						h_attribyte[2] = "SFX to play when changing combo.";
						break;
					case cflag7:
						h_attribyte[2] = "SFX to play when triggering singular secret.";
						break;
					case cflag4|cflag7:
						h_attribyte[2] = "SFX to play when triggering singular secret or changing combo";
						break;
				}
			}
			if(FL(cflag7))
			{
				l_attribyte[3] = "Singular Secret:";
				h_attribyte[3] = "Which single secret combo to trigger, using the 'SECCMB_' constants from 'include/std_zh/std_constants.zh'";
			}
			if(FL(cflag14)) //Drop Enemy flag
			{
				l_flag[12] = "No Poof";
				h_flag[12] = "Skip spawn poof for dropped enemy";
			}
			l_flag[5] = "Room Item";
			h_flag[5] = "Drop the room's Item on trigger";
			break;
		}
		case cPITFALL:
		{
			l_flag[0] = "Warp";
			h_flag[0] = "Warp to another screen using a tile warp when falling";
			l_flag[2] = "Damage is Percent";
			h_flag[2] = "The damage amount is a percentage of the player's max life";
			l_flag[3] = "Allow Ladder";
			h_flag[3] = "A ladder with 'Can step over pitfalls' checked can step over this combo";
			l_flag[4] = "No Pull";
			h_flag[4] = "Don't suck in the player at all";
			l_attribute[0] = "Damage:";
			h_attribute[0] = "The amount of damage, in HP, to take when falling. Negative values heal.";
			l_attribyte[0] = "Fall SFX:";
			h_attribyte[0] = "The SFX to play when falling";
			if(FL(cflag1)) //Warp enabled
			{
				l_flag[1] = "Direct Warp";
				h_flag[1] = "The warp keeps the player at the same x/y position";
				l_attribyte[1] = "TileWarp ID";
				h_attribyte[1] = "0 = A, 1 = B, 2 = C, 3 = D";
			}
			if(!(FL(cflag5))) //"No Pull"
			{
				l_attribyte[2] = "Pull Sensitivity:";
				h_attribyte[2] = "Pull the player 1 pixel every this many frames.\n"
					"If set to 0, pulls 2 pixels every frame.";
			}
			break;
		}
		case cSTEPSFX:
		{
			l_flag[0] = "Landmine (Step->Wpn)";
			h_flag[0] = "Spawns a weapon when triggered, and by default advances to the next combo in the combo list.";
			l_attribyte[0] = "Sound:";
			h_attribyte[0] = "SFX to play when stepped on";
			if(FL(cflag1)) //Landmine
			{
				l_flag[1] = "Script weapon IDs spawn LWeapons";
				h_flag[1] = "Script weapon IDs for 'Weapon Type' are EWeapons by default; if checked, they will be LWeapons instead.";
				l_flag[2] = "Don't Advance";
				h_flag[2] = "If checked, the combo will not advance to the next combo when triggered."
					" This may cause the landmine to trigger multiple times in a row.";
				l_flag[3] = "Direct Damage from Script LWs & Sparkles";
				h_flag[3] = "If the weapon type is a Script weapon and 'Script Weapon IDs spawn LWeapons' is checked, or the weapon type is"
					" a sparkle type, it will immediately damage the player (knocking them back none).";
				l_attribute[0] = "Damage:";
				h_attribute[0] = "The damage value for the spawned weapon. If this is < 1, it will default to 4 damage.";
				l_attribyte[1] = "Weapon Type:";
				h_attribyte[1] = "The weapon type to spawn. Script1-10 weapon types are eweapons by default."
					" If 0 or invalid, uses an enemy bomb type as a default.";
				l_attribyte[2] = "Weapon Dir:";
				h_attribyte[2] = "Direction for the weapon. 0-7 are the standard dirs, 8+ selects a random dir.";
				l_attribyte[3] = "Wpn Sprite:";
				h_attribyte[3] = "The 'Sprite Data' sprite to use for the spawned weapon. Only valid if 1 to 255.";
			}
			break;
		}
		case cCSWITCH:
		{
			l_flag[0] = "Kill Wpn";
			h_flag[0] = "Destroy the weapon that triggers the combo";
			l_flag[7] = "Skip Cycle on Screen Entry";
			h_flag[7] = "Combo cycle the switch combo on screen entry, to skip any switching animation";
			
			l_attribute[0] = "Combo Change:";
			h_attribute[0] = "Value to add to the combo ID when triggered";
			l_attribute[1] = "CSet Change:";
			h_attribute[1] = "Value to add to the cset when triggered";
			l_attribyte[0] = "State Num:";
			h_attribyte[0] = "Range 0-31 inclusive, which of the level's switch states to trigger from";
			l_attribyte[1] = "SFX:";
			h_attribyte[2] = "SFX to play when triggered";
			break;
		}
		case cCSWITCHBLOCK:
		{
			l_flag[0] = "Change L0"; l_flag[1] = "Change L1";
			l_flag[2] = "Change L2"; l_flag[3] = "Change L3";
			l_flag[4] = "Change L4"; l_flag[5] = "Change L5";
			l_flag[6] = "Change L6";
			h_flag[0] = "Changes the combo on layer 0 in the same pos as this combo when triggered.";
			h_flag[1] = "Changes the combo on layer 1 in the same pos as this combo when triggered.";
			h_flag[2] = "Changes the combo on layer 2 in the same pos as this combo when triggered.";
			h_flag[3] = "Changes the combo on layer 3 in the same pos as this combo when triggered.";
			h_flag[4] = "Changes the combo on layer 4 in the same pos as this combo when triggered.";
			h_flag[5] = "Changes the combo on layer 5 in the same pos as this combo when triggered.";
			h_flag[6] = "Changes the combo on layer 6 in the same pos as this combo when triggered.";
			l_flag[7] = "Skip Cycle on Screen Entry";
			h_flag[7] = "Combo cycle the switch combo on screen entry, to skip any rising/falling animation";
			l_flag[8] = "Allow walk-on-top";
			h_flag[7] = "Allows the player to walk along solid switchblocks if they are on them";
			l_attribute[0] = "Combo Change:";
			h_attribute[0] = "Value to add to the combo ID when triggered";
			l_attribute[1] = "CSet Change:";
			h_attribute[1] = "Value to add to the cset when triggered";
			l_attribyte[0] = "State Num:";
			h_attribyte[0] = "Range 0-31 inclusive, which of the level's switch states to trigger from";
			if(FL(cflag9)) //Allow walk-on-top
			{
				l_flag[9] = "-8px DrawYOffset";
				h_flag[9] = "If enabled, when the Player stands atop the block (solid area), the player's DrawYOffset is decremented by 8."
					" When the Player leaves the block, the DrawYOffset is incremented back by 8.";
				l_attribute[2] = "Z-value:";
				h_attribute[2] = "A Z-height for the block, allowing you to jump atop it, and from block to block."
					" If set to 0, acts as infinitely tall.";
				l_attribute[3] = "Step Height:";
				h_attribute[3] = "The Z amount below the block's Z-height that you can jump atop it from. This allows"
					" for 'walking up stairs' type effects.";
			}
			break;
		}
		case cTORCH:
		{
			l_attribyte[0] = "Radius:";
			h_attribyte[0] = "The radius of light, in pixels, to light up in dark rooms.";
			break;
		}
		case cSPOTLIGHT:
		{
			l_flag[0] = "Use Tiles instead of Colors";
			h_flag[0] = "Uses a set of tiles in a preset order, instead of a set of 3 colors, to represent the light beam.";
			l_attribyte[0] = "Dir:";
			h_attribyte[0] = "0-3 = Up,Down,Left,Right\n4-7 = Unused (For Now)\n8 = at the ground";
			l_attribyte[4] = "Trigger Set:";
			h_attribyte[4] = "0-32; if 0 will trigger any targets, otherwise only triggers matching targets";
			if(FL(cflag1))
			{
				l_attribute[0] = "Start Tile:";
				h_attribute[0] = "Tiles in order: Ground, Up, Down, Left, Right, U+L, U+R, D+L, D+R, U+D, L+R, D+L+R, U+L+R, U+D+R, U+D+L, U+D+L+R";
				l_attribyte[1] = "CSet (0-11):";
				h_attribyte[1] = "CSet for the light beam graphic";
			}
			else
			{
				l_attribyte[1] = "Inner Color:";
				h_attribyte[1] = "One of the colors used to generate the light beam graphic";
				l_attribyte[2] = "Middle Color:";
				h_attribyte[2] = "One of the colors used to generate the light beam graphic";
				l_attribyte[3] = "Outer Color:";
				h_attribyte[3] = "One of the colors used to generate the light beam graphic";
			}
			break;
		}
		case cLIGHTTARGET:
		{
			l_flag[0] = "Lit Version";
			h_flag[0] = "If checked, reverts to previous combo when not hit by a spotlight."
				"\nIf unchecked, becomes the next combo when hit by a spotlight.";
			l_flag[1] = "Invert";
			h_flag[1] = "If checked, counts as triggered when light is NOT hitting it.";
			l_attribyte[4] = "Trigger Set:";
			h_attribyte[4] = "0-32; if 0 will be triggered by any beams, otherwise only by matching beams";
			break;
		}
		case cSWITCHHOOK:
		{
			l_flag[0] = "Only swap with Combo 0";
			h_flag[0] = "The switch will fail if this combo would be swapped with a non-zero combo";
			l_flag[1] = "Swap Placed Flags";
			h_flag[1] = "Placed flags on the same layer and position as this combo will be swapped along with the combo";
			l_flag[2] = "Break upon swap";
			h_flag[2] = "The combo will 'break' upon swapping, displaying a break sprite and potentially"
				" dropping an item. Instead of swapping with the combo under the player,"
				" it will be replaced by the screen's Undercombo.";
			l_flag[6] = "Counts as 'pushblock'";
			h_flag[6] = "This combo counts as a 'pushblock' for purposes of switching it onto"
				" block triggers/holes, though this does not allow it to be pushed"
				" (unless a push flag is placed on it).";
			l_attribyte[0] = "Hook Level:";
			h_attribyte[0] = "The minimum level of SwitchHook that can swap this combo";
			if(FL(cflag3)) //break info
			{
				l_attribyte[1] = "Break Sprite:";
				h_attribyte[1] = "Sprite Data sprite ID to display when broken";
				l_attribyte[2] = "Break SFX:";
				h_attribyte[2] = "SFX to be played when broken";
				l_flag[3] = "Drop Item";
				h_flag[3] = "Will drop an item upon breaking.";
				l_flag[5] = "Next instead of Undercombo";
				h_flag[5] = "Replace with the Next combo instead of the screen's Undercombo";
				if(FL(cflag4))
				{
					l_flag[4] = "Specific Item";
					h_flag[4] = "Drop a specific item instead of an item from a dropset";
					if(FL(cflag5))
					{
						l_attribyte[2] = "Item:";
						h_attribyte[2] = "The item ID to drop";
					}
					else
					{
						l_attribyte[2] = "Dropset:";
						h_attribyte[2] = "The dropset to select a drop item from";
					}
				}
			}
			break;
		}
	}
	for(size_t q = 0; q < 16; ++q)
	{
		l_flags[q]->setText(l_flag[q]);
		ib_flags[q]->setDisabled(h_flag[q].empty());
		if(q > 7) continue;
		ib_attribytes[q]->setDisabled(h_attribyte[q].empty());
		l_attribytes[q]->setText(l_attribyte[q]);
		ib_attrishorts[q]->setDisabled(h_attrishort[q].empty());
		l_attrishorts[q]->setText(l_attrishort[q]);
		if(q > 3) continue;
		ib_attributes[q]->setDisabled(h_attribute[q].empty());
		l_attributes[q]->setText(l_attribute[q]);
	}
	pendDraw();
}
void ComboEditorDialog::loadComboFlag()
{
	flaghelp = getFlagHelpText(local_comboref.flag);
}
void ComboEditorDialog::updateCSet()
{
	tswatch->setCSet(CSet);
	if(local_comboref.animflags&AF_CYCLENOCSET)
		cycleswatch->setCSet(CSet);
	else cycleswatch->setCSet(local_comboref.nextcset);
	animFrame->setCSet(CSet);
	l_cset->setText(std::to_string(CSet));
}
void ComboEditorDialog::updateAnimation()
{
	updateCSet();
	animFrame->setCSet2(local_comboref.csets);
	animFrame->setFrames(local_comboref.frames);
	animFrame->setSkipX(local_comboref.skipanim);
	animFrame->setSkipY(local_comboref.skipanimy);
	animFrame->setSpeed(local_comboref.speed);
	animFrame->setTile(local_comboref.tile);
	animFrame->setFlip(local_comboref.flip);
}

//{ Macros

#define DISABLE_WEAP_DATA true
#define ATTR_WID 6_em
#define ATTR_LAB_WID 12_em
#define SPR_LAB_WID sized(14_em,10_em)
#define ACTION_LAB_WID 6_em
#define ACTION_FIELD_WID 6_em
#define FLAGS_WID 16_em

#define NUM_FIELD(member,_min,_max) \
TextField( \
	fitParent = true, \
	type = GUI::TextField::type::INT_DECIMAL, \
	low = _min, high = _max, val = local_comboref.member, \
	onValChangedFunc = [&](GUI::TextField::type,std::string_view,int32_t val) \
	{ \
		local_comboref.member = val; \
	})
	
#define ANIM_FIELD(member,_min,_max) \
TextField( \
	fitParent = true, \
	type = GUI::TextField::type::INT_DECIMAL, \
	low = _min, high = _max, val = local_comboref.member, \
	onValChangedFunc = [&](GUI::TextField::type,std::string_view,int32_t val) \
	{ \
		local_comboref.member = val; \
		updateAnimation(); \
	})

#define CMB_FLAG(ind) \
Row(padding = 0_px, \
	ib_flags[ind] = Button(forceFitH = true, text = "?", \
		disabled = true, \
		onPressFunc = [&]() \
		{ \
			InfoDialog("Flag Info",h_flag[ind]).show(); \
		}), \
	l_flags[ind] = Checkbox( \
		minwidth = FLAGS_WID, hAlign = 0.0, \
		checked = local_comboref.usrflags & (1<<ind), fitParent = true, \
		onToggleFunc = [&](bool state) \
		{ \
			SETFLAG(local_comboref.usrflags,(1<<ind),state); \
			loadComboType(); \
		} \
	) \
)

#define CMB_GEN_FLAG(ind,str) \
Checkbox(text = str, \
		minwidth = FLAGS_WID, hAlign = 0.0, \
		checked = local_comboref.genflags & (1<<ind), fitParent = true, \
		onToggleFunc = [&](bool state) \
		{ \
			SETFLAG(local_comboref.genflags,(1<<ind),state); \
		} \
	)

#define CMB_ATTRIBYTE(ind) \
l_attribytes[ind] = Label(minwidth = ATTR_LAB_WID, textAlign = 2), \
ib_attribytes[ind] = Button(forceFitH = true, text = "?", \
	disabled = true, \
	onPressFunc = [&]() \
	{ \
		InfoDialog("Attribyte Info",h_attribyte[ind]).show(); \
	}), \
TextField( \
	fitParent = true, minwidth = 8_em, \
	type = GUI::TextField::type::SWAP_BYTE, \
	low = 0, high = 255, val = local_comboref.attribytes[ind], \
	onValChangedFunc = [&](GUI::TextField::type,std::string_view,int32_t val) \
	{ \
		local_comboref.attribytes[ind] = val; \
	})

#define CMB_ATTRISHORT(ind) \
l_attrishorts[ind] = Label(minwidth = ATTR_LAB_WID, textAlign = 2), \
ib_attrishorts[ind] = Button(forceFitH = true, text = "?", \
	disabled = true, \
	onPressFunc = [&]() \
	{ \
		InfoDialog("Attrishort Info",h_attrishort[ind]).show(); \
	}), \
TextField( \
	fitParent = true, minwidth = 8_em, \
	type = GUI::TextField::type::SWAP_SSHORT, \
	low = -32768, high = 32767, val = local_comboref.attrishorts[ind], \
	onValChangedFunc = [&](GUI::TextField::type,std::string_view,int32_t val) \
	{ \
		local_comboref.attrishorts[ind] = val; \
	})

#define CMB_ATTRIBUTE(ind) \
l_attributes[ind] = Label(minwidth = ATTR_LAB_WID, textAlign = 2), \
ib_attributes[ind] = Button(forceFitH = true, text = "?", \
	disabled = true, \
	onPressFunc = [&]() \
	{ \
		InfoDialog("Attribute Info",h_attribute[ind]).show(); \
	}), \
TextField( \
	fitParent = true, minwidth = 8_em, \
	type = GUI::TextField::type::SWAP_ZSINT, \
	val = local_comboref.attributes[ind], \
	onValChangedFunc = [&](GUI::TextField::type,std::string_view,int32_t val) \
	{ \
		local_comboref.attributes[ind] = val; \
	})

#define TRIGFLAG(ind, str) \
Checkbox( \
	text = str, hAlign = 0.0, \
	checked = (local_comboref.triggerflags[ind/32] & (1<<(ind%32))), \
	fitParent = true, \
	onToggleFunc = [&](bool state) \
	{ \
		SETFLAG(local_comboref.triggerflags[ind/32],(1<<(ind%32)),state); \
	} \
)

//}

int32_t solidity_to_flag(int32_t val)
{
	return (val&0b1001) | (val&0b0100)>>1 | (val&0b0010)<<1;
}

std::shared_ptr<GUI::Widget> ComboEditorDialog::view()
{
	using namespace GUI::Builder;
	using namespace GUI::Props;
	using namespace GUI::Key;
	
	char titlebuf[256];
	sprintf(titlebuf, "Combo Editor (%d)", index);
	if(is_large)
	{
		window = Window(
			use_vsync = true,
			title = titlebuf,
			info = "Edit combos, setting up their graphics, effects, and attributes.\n"
				"Hotkeys:\n"
				"-/+: Change CSet\n"
				"H/V/R: Flip (Horz,Vert,Rotate)",
			onEnter = message::OK,
			onClose = message::CANCEL,
			shortcuts={
				V=message::VFLIP,
				H=message::HFLIP,
				R=message::ROTATE,
				PlusPad=message::PLUSCS,
				Equals=message::PLUSCS,
				MinusPad=message::MINUSCS,
				Minus=message::MINUSCS,
			},
			Column(
				Rows<3>(padding = 0_px,
					Label(text = "Type:", hAlign = 1.0),
					DropDownList(data = list_ctype, fitParent = true,
						maxwidth = sized(220_px, 400_px),
						padding = 0_px, selectedValue = local_comboref.type,
						onSelectionChanged = message::COMBOTYPE
					),
					Button(
						width = 1.5_em, padding = 0_px, forceFitH = true,
						text = "?", hAlign = 1.0, onPressFunc = [&]()
						{
							InfoDialog(moduledata.combo_type_names[local_comboref.type],typehelp).show();
						}
					),
					Label(text = "Inherent Flag:", hAlign = 1.0),
					DropDownList(data = list_flag, fitParent = true,
						maxwidth = sized(220_px, 400_px),
						padding = 0_px, selectedValue = local_comboref.flag,
						onSelectionChanged = message::COMBOFLAG
					),
					Button(
						width = 1.5_em, padding = 0_px, forceFitH = true,
						text = "?", hAlign = 1.0, onPressFunc = [&]()
						{
							InfoDialog(moduledata.combo_flag_names[local_comboref.flag],flaghelp).show();
						}
					)
				),
				TabPanel(
					ptr = &cmb_tab1,
					TabRef(name = "Basic", Row(
						Rows<2>(
							Label(text = "Label:", hAlign = 1.0),
							TextField(
								fitParent = true,
								type = GUI::TextField::type::TEXT,
								maxLength = 10,
								text = std::string(local_comboref.label),
								onValChangedFunc = [&](GUI::TextField::type,std::string_view text,int32_t)
								{
									std::string foo;
									foo.assign(text);
									strncpy(local_comboref.label, foo.c_str(), 10);
								}),
							Label(text = "CSet 2:", hAlign = 1.0),
							TextField(
								fitParent = true,
								type = GUI::TextField::type::INT_DECIMAL,
								low = -8, high = 7, val = (local_comboref.csets&8) ? ((local_comboref.csets&0xF)|~int32_t(0xF)) : (local_comboref.csets&0xF),
								onValChangedFunc = [&](GUI::TextField::type,std::string_view,int32_t val)
								{
									local_comboref.csets &= ~0xF;
									local_comboref.csets |= val&0xF;
									updateAnimation();
								}),
							Label(text = "A. Frames:", hAlign = 1.0),
							ANIM_FIELD(frames, 0, 255),
							Label(text = "A. Speed:", hAlign = 1.0),
							ANIM_FIELD(speed, 0, 255),
							Label(text = "A. SkipX:", hAlign = 1.0),
							ANIM_FIELD(skipanim, 0, 255),
							Label(text = "A. SkipY:", hAlign = 1.0),
							ANIM_FIELD(skipanimy, 0, 255),
							Label(text = "Flip:", hAlign = 1.0),
							l_flip = Label(text = std::to_string(local_comboref.flip), hAlign = 0.0),
							Label(text = "CSet:", hAlign = 1.0),
							l_cset = Label(text = std::to_string(CSet), hAlign = 0.0)
						),
						Column(padding = 0_px,
							Rows<6>(
								Label(text = "Tile", hAlign = 0.5, colSpan = 2),
								Label(text = "Solid", hAlign = 1.0, rightPadding = 0_px),
								Button(leftPadding = 0_px, forceFitH = true, text = "?",
									onPressFunc = []()
									{
										InfoDialog("Solidity","The pink-highlighted corners of the combo will be treated"
											" as solid walls.").show();
									}),
								Label(text = "CSet2", hAlign = 1.0, rightPadding = 0_px),
								Button(leftPadding = 0_px, forceFitH = true, text = "?",
									onPressFunc = []()
									{
										InfoDialog("CSet2","The cyan-highlighted corners of the combo will be drawn"
											" in a different cset, offset by the value in the 'CSet2' field.").show();
									}),
								tswatch = SelTileSwatch(
									colSpan = 2,
									tile = local_comboref.tile,
									cset = CSet,
									showvals = false,
									onSelectFunc = [&](int32_t t, int32_t c)
									{
										local_comboref.tile = t;
										local_comboref.o_tile = t;
										CSet = (c&0xF)%12;
										updateAnimation();
									}
								),
								cswatchs[0] = CornerSwatch(colSpan = 2,
									val = solidity_to_flag(local_comboref.walk&0xF),
									color = vc(12),
									onSelectFunc = [&](int32_t val)
									{
										local_comboref.walk &= ~0xF;
										local_comboref.walk |= solidity_to_flag(val);
									}
								),
								cswatchs[1] = CornerSwatch(colSpan = 2,
									val = (local_comboref.csets&0xF0)>>4,
									color = vc(11),
									onSelectFunc = [&](int32_t val)
									{
										local_comboref.csets &= ~0xF0;
										local_comboref.csets |= val<<4;
										updateAnimation();
									}
								),
								animFrame = TileFrame(
									colSpan = 2,
									tile = local_comboref.tile,
									cset = CSet,
									cset2 = local_comboref.csets,
									frames = local_comboref.frames,
									speed = local_comboref.speed,
									skipx = local_comboref.skipanim,
									skipy = local_comboref.skipanimy,
									flip = local_comboref.flip
								),
								cycleswatch = SelComboSwatch(colSpan = 2,
									showvals = false,
									combo = local_comboref.nextcombo,
									cset = local_comboref.nextcset,
									onSelectFunc = [&](int32_t cmb, int32_t c)
									{
										local_comboref.nextcombo = cmb;
										local_comboref.nextcset = c;
										updateCSet();
									}
								),
								cswatchs[2] = CornerSwatch(colSpan = 2,
									val = solidity_to_flag((local_comboref.walk&0xF0)>>4),
									color = vc(10),
									onSelectFunc = [&](int32_t val)
									{
										local_comboref.walk &= ~0xF0;
										local_comboref.walk |= solidity_to_flag(val)<<4;
									}
								),
								Label(text = "Preview", hAlign = 0.5,colSpan = 2),
								Label(text = "Cycle", hAlign = 1.0, rightPadding = 0_px),
								Button(leftPadding = 0_px, forceFitH = true, text = "?",
									onPressFunc = []()
									{
										InfoDialog("Cycle","When the combo's animation has completed once,"
											" the combo will be changed to the 'Cycle' combo, unless the 'Cycle'"
											" combo is set to Combo 0.").show();
									}),
								Label(text = "Effect", hAlign = 1.0, rightPadding = 0_px),
								Button(leftPadding = 0_px, forceFitH = true, text = "?",
									onPressFunc = []()
									{
										InfoDialog("Effect","The combo type takes effect only in the lime-highlighted"
											" corners of the combo.").show();
									})
							),
							Checkbox(
								text = "Refresh Animation on Room Entry", hAlign = 0.0,
								checked = local_comboref.animflags & AF_FRESH,
								onToggleFunc = [&](bool state)
								{
									SETFLAG(local_comboref.animflags,AF_FRESH,state);
								}
							),
							Checkbox(
								text = "Restart Animation when Cycled To", hAlign = 0.0,
								checked = local_comboref.animflags & AF_CYCLE,
								onToggleFunc = [&](bool state)
								{
									SETFLAG(local_comboref.animflags,AF_CYCLE,state);
								}
							),
							Checkbox(
								text = "Cycle Ignores CSet", hAlign = 0.0,
								checked = local_comboref.animflags & AF_CYCLENOCSET,
								onToggleFunc = [&](bool state)
								{
									SETFLAG(local_comboref.animflags,AF_CYCLENOCSET,state);
									updateCSet();
								}
							)
						)
					)),
					TabRef(name = "Flags", Column(
						padding = 0_px,
						Rows<2>(
							framed = true,
							frameText = "General Flags",
							topPadding = DEFAULT_PADDING+0.4_em,
							bottomPadding = DEFAULT_PADDING+1_px,
							bottomMargin = 1_em,
							CMB_GEN_FLAG(0,"Hook-Grabbable"),
							CMB_GEN_FLAG(1,"Switch-Hookable")
						),
						Columns<8>(
							framed = true,
							frameText = "Variable Flags",
							topPadding = DEFAULT_PADDING+0.4_em,
							bottomPadding = DEFAULT_PADDING+1_px,
							bottomMargin = 1_em,
							CMB_FLAG(0),
							CMB_FLAG(1),
							CMB_FLAG(2),
							CMB_FLAG(3),
							CMB_FLAG(4),
							CMB_FLAG(5),
							CMB_FLAG(6),
							CMB_FLAG(7),
							CMB_FLAG(8),
							CMB_FLAG(9),
							CMB_FLAG(10),
							CMB_FLAG(11),
							CMB_FLAG(12),
							CMB_FLAG(13),
							CMB_FLAG(14),
							CMB_FLAG(15)
						)
					)),
					TabRef(name = "Attribs", ScrollingPane(
						fitParent = true,
						Rows<6>(
							Label(text = "Attribytes", colSpan = 3),
							Label(text = "Attrishorts", colSpan = 3),
							CMB_ATTRIBYTE(0),
							CMB_ATTRISHORT(0),
							CMB_ATTRIBYTE(1),
							CMB_ATTRISHORT(1),
							CMB_ATTRIBYTE(2),
							CMB_ATTRISHORT(2),
							CMB_ATTRIBYTE(3),
							CMB_ATTRISHORT(3),
							CMB_ATTRIBYTE(4),
							CMB_ATTRISHORT(4),
							CMB_ATTRIBYTE(5),
							CMB_ATTRISHORT(5),
							CMB_ATTRIBYTE(6),
							CMB_ATTRISHORT(6),
							CMB_ATTRIBYTE(7),
							CMB_ATTRISHORT(7),
							Label(text = "Attributes", colSpan = 3), DummyWidget(colSpan = 3),
							CMB_ATTRIBUTE(0), DummyWidget(colSpan = 3),
							CMB_ATTRIBUTE(1), DummyWidget(colSpan = 3),
							CMB_ATTRIBUTE(2), DummyWidget(colSpan = 3),
							CMB_ATTRIBUTE(3), DummyWidget(colSpan = 3)
						)
					)),
					TabRef(name = "Triggers", Row(
						Column(
							padding = 0_px,
							Column(framed = true,
								Row(
									padding = 0_px,
									Label(text = "Min Level (Applies to all):"),
									TextField(
										fitParent = true,
										vPadding = 0_px,
										type = GUI::TextField::type::INT_DECIMAL,
										low = 0, high = 214748, val = local_comboref.triggerlevel,
										onValChangedFunc = [&](GUI::TextField::type,std::string_view,int32_t val)
										{
											local_comboref.triggerlevel = val;
										})
								),
								Rows<4>(
									TRIGFLAG(0,"Sword"),
									TRIGFLAG(1,"Sword Beam"),
									TRIGFLAG(2,"Boomerang"),
									TRIGFLAG(3,"Bomb"),
									TRIGFLAG(4,"Super Bomb"),
									TRIGFLAG(5,"Lit Bomb"),
									TRIGFLAG(6,"Lit Super Bomb"),
									TRIGFLAG(7,"Arrow"),
									TRIGFLAG(8,"Fire"),
									TRIGFLAG(9,"Whistle"),
									TRIGFLAG(10,"Bait"),
									TRIGFLAG(11,"Wand"),
									TRIGFLAG(12,"Magic"),
									TRIGFLAG(13,"Wind"),
									TRIGFLAG(14,"Refl. Magic"),
									TRIGFLAG(15,"Refl. Fireball"),
									TRIGFLAG(16,"Refl. Rock"),
									TRIGFLAG(17,"Hammer"),
									TRIGFLAG(32,"Hookshot"),
									TRIGFLAG(33,"Sparkle"),
									TRIGFLAG(34,"Byrna"),
									TRIGFLAG(35,"Refl. Beam"),
									TRIGFLAG(36,"Stomp"),
									DummyWidget(),
									TRIGFLAG(37,"Custom Weapon 1"),
									TRIGFLAG(38,"Custom Weapon 2"),
									TRIGFLAG(39,"Custom Weapon 3"),
									TRIGFLAG(40,"Custom Weapon 4"),
									TRIGFLAG(41,"Custom Weapon 5"),
									TRIGFLAG(42,"Custom Weapon 6"),
									TRIGFLAG(43,"Custom Weapon 7"),
									TRIGFLAG(44,"Custom Weapon 8"),
									TRIGFLAG(45,"Custom Weapon 9"),
									TRIGFLAG(46,"Custom Weapon 10")
								)
							),
							Rows<4>(
								framed = true,
								TRIGFLAG(48,"Triggers Secrets"),
								TRIGFLAG(18,"->Next"),
								TRIGFLAG(19,"->Prev")
							)
						),
						Column(framed = true,
							Row(padding = 0_px,
								Label(text = "Buttons:"),
								TextField(
									fitParent = true,
									vPadding = 0_px,
									type = GUI::TextField::type::INT_DECIMAL,
									low = 0, high = 255, val = local_comboref.triggerbtn,
									onValChangedFunc = [&](GUI::TextField::type,std::string_view,int32_t val)
									{
										local_comboref.triggerbtn = val;
									}),
								Button(
									width = 1.5_em, padding = 0_px, forceFitH = true,
									text = "?", hAlign = 1.0, onPressFunc = [&]()
									{
										InfoDialog("Button Triggers","Sum all the buttons you want to be usable:\n"
											"(A=1, B=2, L=4, R=8, Ex1=16, Ex2=32, Ex3=64, Ex4=128)\n"
											"Buttons used while standing against the combo from a direction"
											" with the 'Btn: [dir]' flag checked for that side"
											" will trigger the combo.").show();
									}
								)
							),
							Column(
								TRIGFLAG(20,"Btn: Top"),
								TRIGFLAG(21,"Btn: Bottom"),
								TRIGFLAG(22,"Btn: Left"),
								TRIGFLAG(23,"Btn: Right"),
								TRIGFLAG(47,"Always Triggered")
							)
						)
					)),
					TabRef(name = "Script", Column(
						INITD_ROW2(0, local_comboref.initd),
						INITD_ROW2(1, local_comboref.initd),
						Row(
							padding = 0_px,
							SCRIPT_LIST("Combo Script:", list_combscript, local_comboref.script)
						)
					))
				),
				Row(
					vAlign = 1.0,
					spacing = 2_em,
					Button(
						focused = true,
						text = "OK",
						minwidth = 90_lpx,
						onClick = message::OK),
					Button(
						text = "Cancel",
						minwidth = 90_lpx,
						onClick = message::CANCEL),
					Button(
						text = "Clear",
						minwidth = 90_lpx,
						onClick = message::CLEAR)
				)
			)
		);
	}
	else
	{
		window = Window(
			use_vsync = true,
			title = titlebuf,
			info = "Edit combos, setting up their graphics, effects, and attributes.\n"
				"Hotkeys:\n"
				"-/+: Change CSet\n"
				"H/V/R: Flip (Horz,Vert,Rotate)",
			onEnter = message::OK,
			onClose = message::CANCEL,
			shortcuts={
				V=message::VFLIP,
				H=message::HFLIP,
				R=message::ROTATE,
				PlusPad=message::PLUSCS,
				Equals=message::PLUSCS,
				MinusPad=message::MINUSCS,
				Minus=message::MINUSCS,
			},
			Column(
				Rows<3>(padding = 0_px,
					Label(text = "Type:", hAlign = 1.0),
					DropDownList(data = list_ctype, fitParent = true,
						maxwidth = sized(220_px, 400_px),
						padding = 0_px, selectedValue = local_comboref.type,
						onSelectionChanged = message::COMBOTYPE
					),
					Button(
						width = 1.5_em, padding = 0_px, forceFitH = true,
						text = "?", hAlign = 1.0, onPressFunc = [&]()
						{
							InfoDialog(moduledata.combo_type_names[local_comboref.type],typehelp).show();
						}
					),
					Label(text = "Inherent Flag:", hAlign = 1.0),
					DropDownList(data = list_flag, fitParent = true,
						maxwidth = sized(220_px, 400_px),
						padding = 0_px, selectedValue = local_comboref.flag,
						onSelectionChanged = message::COMBOFLAG
					),
					Button(
						width = 1.5_em, padding = 0_px, forceFitH = true,
						text = "?", hAlign = 1.0, onPressFunc = [&]()
						{
							InfoDialog(moduledata.combo_type_names[local_comboref.type],flaghelp).show();
						}
					)
				),
				TabPanel(
					ptr = &cmb_tab1,
					TabRef(name = "Basic", Row(
						Rows<2>(
							Label(text = "Label:", hAlign = 1.0),
							TextField(
								fitParent = true,
								type = GUI::TextField::type::TEXT,
								maxLength = 10,
								text = std::string(local_comboref.label),
								onValChangedFunc = [&](GUI::TextField::type,std::string_view text,int32_t)
								{
									std::string foo;
									foo.assign(text);
									strncpy(local_comboref.label, foo.c_str(), 10);
								}),
							Label(text = "CSet 2:", hAlign = 1.0),
							TextField(
								fitParent = true,
								type = GUI::TextField::type::INT_DECIMAL,
								low = -8, high = 7, val = (local_comboref.csets&8) ? ((local_comboref.csets&0xF)|~int32_t(0xF)) : (local_comboref.csets&0xF),
								onValChangedFunc = [&](GUI::TextField::type,std::string_view,int32_t val)
								{
									local_comboref.csets &= ~0xF;
									local_comboref.csets |= val&0xF;
									updateAnimation();
								}),
							Label(text = "A. Frames:", hAlign = 1.0),
							ANIM_FIELD(frames, 0, 255),
							Label(text = "A. Speed:", hAlign = 1.0),
							ANIM_FIELD(speed, 0, 255),
							Label(text = "A. SkipX:", hAlign = 1.0),
							ANIM_FIELD(skipanim, 0, 255),
							Label(text = "A. SkipY:", hAlign = 1.0),
							ANIM_FIELD(skipanimy, 0, 255),
							Label(text = "Flip:", hAlign = 1.0),
							l_flip = Label(text = std::to_string(local_comboref.flip), hAlign = 0.0),
							Label(text = "CSet:", hAlign = 1.0),
							l_cset = Label(text = std::to_string(CSet), hAlign = 0.0)
						),
						Column(padding = 0_px,
							Rows<6>(
								Label(text = "Tile", hAlign = 0.5, colSpan = 2),
								Label(text = "Solid", hAlign = 1.0, rightPadding = 0_px),
								Button(leftPadding = 0_px, forceFitH = true, text = "?",
									onPressFunc = []()
									{
										InfoDialog("Solidity","The pink-highlighted corners of the combo will be treated"
											" as solid walls.").show();
									}),
								Label(text = "CSet2", hAlign = 1.0, rightPadding = 0_px),
								Button(leftPadding = 0_px, forceFitH = true, text = "?",
									onPressFunc = []()
									{
										InfoDialog("CSet2","The cyan-highlighted corners of the combo will be drawn"
											" in a different cset, offset by the value in the 'CSet2' field.").show();
									}),
								tswatch = SelTileSwatch(
									colSpan = 2,
									tile = local_comboref.tile,
									cset = CSet,
									showvals = false,
									onSelectFunc = [&](int32_t t, int32_t c)
									{
										local_comboref.tile = t;
										local_comboref.o_tile = t;
										CSet = (c&0xF)%12;
										updateAnimation();
									}
								),
								cswatchs[0] = CornerSwatch(colSpan = 2,
									val = solidity_to_flag(local_comboref.walk&0xF),
									color = vc(12),
									onSelectFunc = [&](int32_t val)
									{
										local_comboref.walk &= ~0xF;
										local_comboref.walk |= solidity_to_flag(val);
									}
								),
								cswatchs[1] = CornerSwatch(colSpan = 2,
									val = (local_comboref.csets&0xF0)>>4,
									color = vc(11),
									onSelectFunc = [&](int32_t val)
									{
										local_comboref.csets &= ~0xF0;
										local_comboref.csets |= val<<4;
										updateAnimation();
									}
								),
								animFrame = TileFrame(
									colSpan = 2,
									tile = local_comboref.tile,
									cset = CSet,
									cset2 = local_comboref.csets,
									frames = local_comboref.frames,
									speed = local_comboref.speed,
									skipx = local_comboref.skipanim,
									skipy = local_comboref.skipanimy,
									flip = local_comboref.flip
								),
								cycleswatch = SelComboSwatch(colSpan = 2,
									showvals = false,
									combo = local_comboref.nextcombo,
									cset = local_comboref.nextcset,
									onSelectFunc = [&](int32_t cmb, int32_t c)
									{
										local_comboref.nextcombo = cmb;
										local_comboref.nextcset = c;
										updateCSet();
									}
								),
								cswatchs[2] = CornerSwatch(colSpan = 2,
									val = solidity_to_flag((local_comboref.walk&0xF0)>>4),
									color = vc(10),
									onSelectFunc = [&](int32_t val)
									{
										local_comboref.walk &= ~0xF0;
										local_comboref.walk |= solidity_to_flag(val)<<4;
									}
								),
								Label(text = "Preview", hAlign = 0.5,colSpan = 2),
								Label(text = "Cycle", hAlign = 1.0, rightPadding = 0_px),
								Button(leftPadding = 0_px, forceFitH = true, text = "?",
									onPressFunc = []()
									{
										InfoDialog("Cycle","When the combo's animation has completed once,"
											" the combo will be changed to the 'Cycle' combo, unless the 'Cycle'"
											" combo is set to Combo 0.").show();
									}),
								Label(text = "Effect", hAlign = 1.0, rightPadding = 0_px),
								Button(leftPadding = 0_px, forceFitH = true, text = "?",
									onPressFunc = []()
									{
										InfoDialog("Effect","The combo type takes effect only in the lime-highlighted"
											" corners of the combo.").show();
									})
							),
							Checkbox(
								text = "Refresh Animation on Room Entry", hAlign = 0.0,
								checked = local_comboref.animflags & AF_FRESH,
								onToggleFunc = [&](bool state)
								{
									SETFLAG(local_comboref.animflags,AF_FRESH,state);
								}
							),
							Checkbox(
								text = "Restart Animation when Cycled To", hAlign = 0.0,
								checked = local_comboref.animflags & AF_CYCLE,
								onToggleFunc = [&](bool state)
								{
									SETFLAG(local_comboref.animflags,AF_CYCLE,state);
								}
							),
							Checkbox(
								text = "Cycle Ignores CSet", hAlign = 0.0,
								checked = local_comboref.animflags & AF_CYCLENOCSET,
								onToggleFunc = [&](bool state)
								{
									SETFLAG(local_comboref.animflags,AF_CYCLENOCSET,state);
									updateCSet();
								}
							)
						)
					)),
					TabRef(name = "Flags", Column(
						padding = 0_px,
						Rows<2>(
							framed = true,
							frameText = "General Flags",
							topPadding = DEFAULT_PADDING+0.4_em,
							bottomPadding = DEFAULT_PADDING+1_px,
							bottomMargin = 1_em,
							CMB_GEN_FLAG(0,"Hook-Grabbable"),
							CMB_GEN_FLAG(1,"Switch-Hookable")
						),
						Columns<8>(
							framed = true,
							frameText = "Variable Flags",
							topPadding = DEFAULT_PADDING+0.4_em,
							bottomPadding = DEFAULT_PADDING+1_px,
							bottomMargin = 1_em,
							CMB_FLAG(0),
							CMB_FLAG(1),
							CMB_FLAG(2),
							CMB_FLAG(3),
							CMB_FLAG(4),
							CMB_FLAG(5),
							CMB_FLAG(6),
							CMB_FLAG(7),
							CMB_FLAG(8),
							CMB_FLAG(9),
							CMB_FLAG(10),
							CMB_FLAG(11),
							CMB_FLAG(12),
							CMB_FLAG(13),
							CMB_FLAG(14),
							CMB_FLAG(15)
						)
					)),
					TabRef(name = "Attribs", TabPanel(
							ptr = &cmb_tab2,
							TabRef(name = "Bytes", ScrollingPane(fitParent = true,
								Rows<3>(
									CMB_ATTRIBYTE(0),
									CMB_ATTRIBYTE(1),
									CMB_ATTRIBYTE(2),
									CMB_ATTRIBYTE(3),
									CMB_ATTRIBYTE(4),
									CMB_ATTRIBYTE(5),
									CMB_ATTRIBYTE(6),
									CMB_ATTRIBYTE(7)
								)
							)),
							TabRef(name = "Shorts", ScrollingPane(fitParent = true,
								Rows<3>(
									CMB_ATTRISHORT(0),
									CMB_ATTRISHORT(1),
									CMB_ATTRISHORT(2),
									CMB_ATTRISHORT(3),
									CMB_ATTRISHORT(4),
									CMB_ATTRISHORT(5),
									CMB_ATTRISHORT(6),
									CMB_ATTRISHORT(7)
								)
							)),
							TabRef(name = "Attributes", ScrollingPane(fitParent = true,
								Rows<3>(
									CMB_ATTRIBUTE(0),
									CMB_ATTRIBUTE(1),
									CMB_ATTRIBUTE(2),
									CMB_ATTRIBUTE(3)
								)
							))
					)),
					TabRef(name = "Triggers", ScrollingPane(
						Column(
							DummyWidget(),
							Column(
								framed = true,
								margins = DEFAULT_PADDING,
								padding = DEFAULT_PADDING+2_px,
								Row(
									Label(text = "Min Level (Applies to all):"),
									TextField(
										fitParent = true,
										type = GUI::TextField::type::INT_DECIMAL,
										low = 0, high = 214748, val = local_comboref.triggerlevel,
										onValChangedFunc = [&](GUI::TextField::type,std::string_view,int32_t val)
										{
											local_comboref.triggerlevel = val;
										})
								),
								Rows<3>(
									TRIGFLAG(0,"Sword"),
									TRIGFLAG(1,"Sword Beam"),
									TRIGFLAG(2,"Boomerang"),
									TRIGFLAG(3,"Bomb"),
									TRIGFLAG(4,"Super Bomb"),
									TRIGFLAG(5,"Lit Bomb"),
									TRIGFLAG(6,"Lit Super Bomb"),
									TRIGFLAG(7,"Arrow"),
									TRIGFLAG(8,"Fire"),
									TRIGFLAG(9,"Whistle"),
									TRIGFLAG(10,"Bait"),
									TRIGFLAG(11,"Wand"),
									TRIGFLAG(12,"Magic"),
									TRIGFLAG(13,"Wind"),
									TRIGFLAG(14,"Refl. Magic"),
									TRIGFLAG(15,"Refl. Fireball"),
									TRIGFLAG(16,"Refl. Rock"),
									TRIGFLAG(17,"Hammer"),
									TRIGFLAG(32,"Hookshot"),
									TRIGFLAG(33,"Sparkle"),
									TRIGFLAG(34,"Byrna"),
									TRIGFLAG(35,"Refl. Beam"),
									TRIGFLAG(36,"Stomp"),
									DummyWidget(),
									TRIGFLAG(37,"C. Weapon 1"),
									TRIGFLAG(38,"C. Weapon 2"),
									TRIGFLAG(39,"C. Weapon 3"),
									TRIGFLAG(40,"C. Weapon 4"),
									TRIGFLAG(41,"C. Weapon 5"),
									TRIGFLAG(42,"C. Weapon 6"),
									TRIGFLAG(43,"C. Weapon 7"),
									TRIGFLAG(44,"C. Weapon 8"),
									TRIGFLAG(45,"C. Weapon 9"),
									TRIGFLAG(46,"C. Weapon 10")
								)
							),
							Column(
								framed = true,
								margins = DEFAULT_PADDING,
								padding = DEFAULT_PADDING+2_px,
								Row(padding = 0_px,
									Label(text = "Buttons:"),
									TextField(
										fitParent = true,
										vPadding = 0_px,
										type = GUI::TextField::type::INT_DECIMAL,
										low = 0, high = 255, val = local_comboref.triggerbtn,
										onValChangedFunc = [&](GUI::TextField::type,std::string_view,int32_t val)
										{
											local_comboref.triggerbtn = val;
										}),
									Button(
										width = 1.5_em, padding = 0_px, forceFitH = true,
										text = "?", hAlign = 1.0, onPressFunc = [&]()
										{
											InfoDialog("Button Triggers","Sum all the buttons you want to be usable:\n"
												"(A=1, B=2, L=4, R=8, Ex1=16, Ex2=32, Ex3=64, Ex4=128)\n"
												"Buttons used while standing against the combo from a direction"
												" with the 'Btn: [dir]' flag checked for that side"
												" will trigger the combo.").show();
										}
									)
								),
								Rows<2>(
									TRIGFLAG(20,"Btn: Top"),
									TRIGFLAG(21,"Btn: Bottom"),
									TRIGFLAG(22,"Btn: Left"),
									TRIGFLAG(23,"Btn: Right"),
									TRIGFLAG(47,"Always Triggered")
								)
							),
							Row(
								framed = true,
								margins = DEFAULT_PADDING,
								padding = DEFAULT_PADDING+2_px,
								TRIGFLAG(48,"Triggers Secrets"),
								TRIGFLAG(18,"->Next"),
								TRIGFLAG(19,"->Prev")
							)
						)
					)),
					TabRef(name = "Script", Column(
						INITD_ROW2(0, local_comboref.initd),
						INITD_ROW2(1, local_comboref.initd),
						Row(
							padding = 0_px,
							SCRIPT_LIST("Combo Script:", list_combscript, local_comboref.script)
						)
					))
				),
				Row(
					vAlign = 1.0,
					spacing = 2_em,
					Button(
						focused = true,
						text = "OK",
						minwidth = 90_lpx,
						onClick = message::OK),
					Button(
						text = "Cancel",
						minwidth = 90_lpx,
						onClick = message::CANCEL),
					Button(
						text = "Clear",
						minwidth = 90_lpx,
						onClick = message::CLEAR)
				)
			)
		);
	}
	loadComboType();
	loadComboFlag();
	return window;
}

bool ComboEditorDialog::handleMessage(const GUI::DialogMessage<message>& msg)
{
	switch(msg.message)
	{
		case message::COMBOTYPE:
		{
			local_comboref.type = int32_t(msg.argument);
			loadComboType();
			return false;
		}
		case message::COMBOFLAG:
		{
			local_comboref.flag = int32_t(msg.argument);
			loadComboFlag();
			return false;
		}
		case message::HFLIP:
		{
			if(cmb_tab1) break;
			local_comboref.flip ^= 1;
			for(auto crn : cswatchs)
			{
				int32_t val = crn->getVal();
				crn->setVal((val & 0b0101)<<1 | (val&0b1010)>>1);
			}
			l_flip->setText(std::to_string(local_comboref.flip));
			updateAnimation();
			return false;
		}
		case message::VFLIP:
		{
			if(cmb_tab1) break;
			local_comboref.flip ^= 2;
			for(auto crn : cswatchs)
			{
				int32_t val = crn->getVal();
				crn->setVal((val & 0b0011)<<2 | (val&0b1100)>>2);
			}
			l_flip->setText(std::to_string(local_comboref.flip));
			updateAnimation();
			return false;
		}
		case message::ROTATE:
		{
			if(cmb_tab1) break;
			local_comboref.flip = rotate_value(local_comboref.flip);
			for(auto crn : cswatchs)
			{
				int32_t val = crn->getVal();
				int32_t newval = 0;
				if(val&0b0001)
					newval |= 0b0010;
				if(val&0b0010)
					newval |= 0b1000;
				if(val&0b0100)
					newval |= 0b0001;
				if(val&0b1000)
					newval |= 0b0100;
				crn->setVal(newval);
			}
			l_flip->setText(std::to_string(local_comboref.flip));
			updateAnimation();
			return false;
		}
		case message::PLUSCS:
		{
			if(cmb_tab1) break;
			CSet = (CSet+1)%12;
			updateCSet();
			return false;
		}
		case message::MINUSCS:
		{
			if(cmb_tab1) break;
			CSet = (CSet+11)%12;
			updateCSet();
			return false;
		}
		case message::CLEAR:
			AlertDialog("Are you sure?",
				"Clearing the combo will reset all values",
				[&](bool ret)
				{
					cleared = ret;
				}).show();
			if(cleared) return true;
			break;
		case message::OK:
			saved = false;
			memcpy(&combobuf[index], &local_comboref, sizeof(local_comboref));
			edited = true;
			return true;

		case message::CANCEL:
		default:
			return true;
	}
	return false;
}

