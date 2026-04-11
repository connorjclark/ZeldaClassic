#include "zc/scripting/types.h"
#include "components/zasm/defines.h"
#include "components/zasm/table.h"

static bool register_routing_table_init;
EngineSubsystem register_routing_table[MAX_REGISTER_ID + 1];

void initializeRegisterRoutingTable()
{
	if (register_routing_table_init)
		return;

	for (int i = 0; i < MAX_REGISTER_ID; i++)
	{
		switch (i)
		{
			case CLASS_THISKEY:
			case CLASS_THISKEY2:
			case GDD:
			case PC:
			case REFBITMAP:
			case REFBOTTLESHOP:
			case REFBOTTLETYPE:
			case REFCOMBODATA:
			case REFCOMBOTRIGGER:
			case REFDIRECTORY:
			case REFDMAPDATA:
			case REFDROPSETDATA:
			case REFEWPN:
			case REFFFC:
			case REFFILE:
			case REFGENERICDATA:
			case REFITEM:
			case REFITEMDATA:
			case REFLWPN:
			case REFMAPDATA:
			case REFMSGDATA:
			case REFMUSIC:
			case REFNPC:
			case REFNPCDATA:
			case REFPALDATA:
			case REFPORTAL:
			case REFRNG:
			case REFSAVEMENU:
			case REFSAVPORTAL:
			case REFSCREEN:
			case REFSHOPDATA:
			case REFSPRITE:
			case REFSPRITEDATA:
			case REFSTACK:
			case REFSUBSCREENDATA:
			case REFSUBSCREENPAGE:
			case REFSUBSCREENWIDG:
			case REFWEBSOCKET:
			case SP:
			case SP2:
			case SWITCHKEY:
				register_routing_table[i] = EngineSubsystem::misc;
				continue;

			case ITEMCOUNT:
				register_routing_table[i] = EngineSubsystem::itemsprite;
				continue;

			case ACTIVESSSPEED:
			case ALLOCATEBITMAPR:
			case COMBOCDM:
			case COMBODDM:
			case COMBOFDM:
			case COMBOIDM:
			case COMBOSDM:
			case COMBOTDM:
			case CURDMAP:
			case CURDSCR:
			case CURLEVEL:
			case CURMAP:
			case CURSCR:
			case GAME_SAVEMENU_F6:
			case GAME_SAVEMENU_GAMEOVER:
			case GAMECHEAT:
			case GAMECLICKFREEZE:
			case GAMECONTDMAP:
			case GAMECONTSCR:
			case GAMEDEATHS:
			case GAMEENTRDMAP:
			case GAMEENTRSCR:
			case GAMEHASPLAYED:
			case GAMEMAXCHEAT:
			case GAMEMAXMAPS:
			case GAMEMOUSECURSOR:
			case GAMENUMMESSAGES:
			case GAMESTANDALONE:
			case GAMETIME:
			case GAMETIMEVALID:
			case GETMIDI:
			case HERO_SCREEN:
			case NOACTIVESUBSC:
			case SCREENSTATEDD:
			case SKIPCREDITS:
			case SKIPF6:
			case TYPINGMODE:
			case ZELDABETA:
			case ZELDABETATYPE:
			case ZELDABUILD:
			case ZELDAVERSION:
			case ZSCRIPTVERSION:
				register_routing_table[i] = EngineSubsystem::game;
				continue;
		}

		if (auto ref = get_register_ref_dependency(i))
		{
			switch (*ref)
			{
				case REFDIRECTORY: register_routing_table[i] = EngineSubsystem::directory; break;
				case REFFILE: register_routing_table[i] = EngineSubsystem::file; break;
				case REFITEM: register_routing_table[i] = EngineSubsystem::itemsprite; break;
				case REFMUSIC: register_routing_table[i] = EngineSubsystem::musicdata; break;
				case REFNPC: register_routing_table[i] = EngineSubsystem::npc; break;
				case REFSAVEMENU: register_routing_table[i] = EngineSubsystem::savemenu; break;
				case REFSPRITE: register_routing_table[i] = EngineSubsystem::sprite; break;
				case REFWEBSOCKET: register_routing_table[i] = EngineSubsystem::websocket; break;
			}
		}
	}

	register_routing_table_init = true;
}
