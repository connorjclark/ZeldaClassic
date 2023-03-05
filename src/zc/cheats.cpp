#include "cheats.h"
#include "zelda.h"
#include "hero.h"
#include "guys.h"
#include "zc_init.h"
#include "init.h"
#include <queue>
#include <tuple>
#include <algorithm>
#include <fmt/format.h>

extern HeroClass Hero;

int32_t cheatkeys[Cheat::Last][2];

static std::queue<std::tuple<Cheat, int, int, std::string>> cheats;

static const std::vector<std::string> CheatStrings = {
	"None",
	"Life",
	"Magic",
	"Rupies",
	"Bombs",
	"Arrows",
	"Kill",
	"Fast",
	"Clock",
	"Walls",
	"Freeze",
	"Light",
	"GoTo",
	"IgnoreSideView",
	"MaxLife",
	"MaxMagic",
	"MaxBombs",
	"PlayerData",
	"TrigSecrets",
	"TrigSecretsPerm",
	"ShowL0",
	"ShowL1",
	"ShowL2",
	"ShowL3",
	"ShowL4",
	"ShowL5",
	"ShowL6",
	"ShowFFC",
	"ShowSprites",
	"ShowWalkability",
	"ShowEffects",
	"ShowOverhead",
	"ShowPushblock",
	"ShowHitbox",
	"ShowFFCScripts",
	"ShowInfoOpacity",
	"Last",
};

Cheat cheat_from_string(std::string name)
{
    auto it = std::find(std::begin(CheatStrings), std::end(CheatStrings), name.c_str());
    ASSERT(it != std::end(CheatStrings));
    return (Cheat)(it - std::begin(CheatStrings));
}

std::string cheat_to_string(Cheat cheat)
{
    return CheatStrings[cheat];
}

std::string cheat_help(Cheat cheat)
{
	switch(cheat)
	{
		case Life:
			return fmt::format("Refill your {} to full", ZI.getCtrName(crLIFE));
		case Magic:
			return fmt::format("Refill your {} to full", ZI.getCtrName(crMAGIC));
		case Rupies:
			return fmt::format("Refill your {} to full", ZI.getCtrName(crMONEY));
		case Bombs:
			return fmt::format("Refill your {} to full", ZI.getCtrName(crBOMBS));
		case Arrows:
			return fmt::format("Refill your {} to full", ZI.getCtrName(crARROWS));
		case Kill:
			return "Kill all enemies on the screen";
		case Fast:
			return "Make the player go 8 times as fast";
		case Clock:
			return "Toggle Invincibility";
		case Walls:
			return "Toggle Walk-Through-Walls";
		case Freeze:
			return "Toggle the player's frozen status?";
		case Light:
			return "Toggle the room's light status";
		case GoTo:
			return "Select a location (dmap,screen) to warp to";
		case IgnoreSideView:
			return "Toggle ignoring sideview gravity";
		case MaxLife:
			return fmt::format("Popup a box to modify your max {}", ZI.getCtrName(crLIFE));
		case MaxMagic:
			return fmt::format("Popup a box to modify your max {}", ZI.getCtrName(crMAGIC));
		case MaxBombs:
			return fmt::format("Popup a box to modify your max {}", ZI.getCtrName(crBOMBS));
		case PlayerData:
			return "Open the Player Data dialog";
		case TrigSecrets:
			return "Trigger current screen secrets (Temp)";
		case TrigSecretsPerm:
			return "Trigger current screen secrets (Perm)";
		case ShowL0:
			return "Toggle visibility of layer 0";
		case ShowL1:
			return "Toggle visibility of layer 1";
		case ShowL2:
			return "Toggle visibility of layer 2";
		case ShowL3:
			return "Toggle visibility of layer 3";
		case ShowL4:
			return "Toggle visibility of layer 4";
		case ShowL5:
			return "Toggle visibility of layer 5";
		case ShowL6:
			return "Toggle visibility of layer 6";
		case ShowFFC:
			return "Toggle visibility of FFCs";
		case ShowSprites:
			return "Toggle visibility of Sprites";
		case ShowWalkability:
			return "Toggle showing walkability debug info";
		case ShowEffects:
			return "Toggle showing effect debug info";
		case ShowOverhead:
			return "Toggle visibility of overhead combos";
		case ShowPushblock:
			return "Toggle visibility of pushblocks";
		case ShowHitbox:
			return "Toggle showing hitbox debug info";
		case ShowFFCScripts:
			return "Toggle listing currently running FFC scripts";
		case ShowInfoOpacity:
			return "Select the opacity of debug info, 0-255";
	}
	return "";
}

int cheat_lvl(Cheat cheat)
{
	switch(cheat)
	{
		case Life:
		case Magic:
		case Rupies:
		case Bombs:
		case Arrows:
			return 1;
		case Clock:
		case MaxLife:
		case MaxMagic:
		case MaxBombs:
			return 2;
		case PlayerData:
			return 3;
		default:
			return 4;
	}
}
bool bindable_cheat(Cheat cheat)
{
	switch(cheat)
	{
		case None:
		case Last:
			return false;
	}
	return true;
}

static void cheats_execute(Cheat cheat, int arg1, int arg2, std::string arg3)
{
    switch (cheat)
    {
    case Life:
    {
        game->set_life(arg1);
    }
    break;

    case Magic:
    {
        game->set_magic(arg1);
    }
    break;

    case Rupies:
    {
        game->set_drupy(arg1);
    }
    break;

    case Bombs:
    {
        for (int32_t i = 0; i < MAXITEMS; i++)
        {
            if (itemsbuf[i].family == itype_bomb || itemsbuf[i].family == itype_sbomb)
                getitem(i, true);
        }

        game->set_bombs(arg1);
        if (arg2 != -1)
            game->set_sbombs(arg2);
    }
    break;

    case Arrows:
    {
        game->set_arrows(arg1);
    }
    break;

    case Kill:
    {
        for (int32_t i = 0; i < guys.Count(); i++)
        {
            if (!(((enemy *)guys.spr(i))->flags & guy_doesntcount))
                ((enemy *)guys.spr(i))->kickbucket();
        }
    }
    break;

    case Fast:
    {
        gofast = gofast ? false : true;
    }
    break;

    case Clock:
    {
        setClock(!getClock());
        cheat_superman = getClock();
    }
    break;

    case Walls:
    {
        toogam = !toogam;
        if (toogam)
        {
            cheat_superman = true;
            setClock(true);
        }
    }
    break;

    case Freeze:
    {
        if (Hero.getAction() == freeze)
        {
            Hero.unfreeze();
        }
        else
        {
            Hero.Freeze();
        }
    }
    break;

    case Light:
    {
        cheats_execute_light = true;
    }
    break;

    case GoTo:
    {
        cheats_execute_goto = true;
        cheat_goto_dmap = arg1;
        cheat_goto_screen = arg2;
    }
    break;

    case IgnoreSideView:
    {
        ignoreSideview = !ignoreSideview;
    }
    break;

    case MaxLife:
    {
        game->set_maxlife(arg1);
    }
    break;

    case MaxMagic:
    {
        game->set_maxmagic(arg1);
    }
    break;

    case MaxBombs:
    {
        game->set_maxbombs(arg1);
    }
    break;

    case PlayerData:
    {
		zinitdata* base = copyIntoZinit(game);
		zinitdata* new_init = apply_init_data_delta(base, arg3);
		if (new_init)
		{
			resetItems(game, new_init, false);
			ringcolor(false);
			delete new_init;
		}
		delete base;
    }
    break;
	
	case TrigSecrets:
	{
		hidden_entrance(0, true, false, -9);
	}
	break;
	case TrigSecretsPerm:
	{
		hidden_entrance(0, true, false, -10);
		setmapflag(mSECRET);
	}
	break;
	case ShowL0:
	{
		onShowLayer0();
	}
	break;
	case ShowL1:
	{
		onShowLayer1();
	}
	break;
	case ShowL2:
	{
		onShowLayer2();
	}
	break;
	case ShowL3:
	{
		onShowLayer3();
	}
	break;
	case ShowL4:
	{
		onShowLayer4();
	}
	break;
	case ShowL5:
	{
		onShowLayer5();
	}
	break;
	case ShowL6:
	{
		onShowLayer6();
	}
	break;
	case ShowFFC:
	{
		onShowLayerF();
	}
	break;
	case ShowSprites:
	{
		onShowLayerS();
	}
	break;
	case ShowWalkability:
	{
		onShowLayerW();
	}
	break;
	case ShowEffects:
	{
		onShowLayerE();
	}
	break;
	case ShowOverhead:
	{
		onShowLayerO();
	}
	break;
	case ShowPushblock:
	{
		onShowLayerP();
	}
	break;
	case ShowHitbox:
	{
		onShowHitboxes();
	}
	break;
	case ShowFFCScripts:
	{
		onShowFFScripts();
	}
	break;
	case ShowInfoOpacity:
	{
		onShowInfoOpacity();
	}
	break;

    case None:
    case Last:
        break;
    }
}

static bool instant_exec(Cheat cheat)
{
	switch(cheat)
	{
		case ShowL0:
		case ShowL1:
		case ShowL2:
		case ShowL3:
		case ShowL4:
		case ShowL5:
		case ShowL6:
		case ShowFFC:
		case ShowSprites:
		case ShowWalkability:
		case ShowEffects:
		case ShowOverhead:
		case ShowPushblock:
		case ShowHitbox:
		case ShowFFCScripts:
		case ShowInfoOpacity:
			return true;
	}
	return false;
}

void cheats_enqueue(Cheat cheat, int arg1, int arg2, std::string arg3)
{
	if(instant_exec(cheat))
		cheats_execute(cheat, arg1, arg2, arg3);
    else cheats.push({cheat, arg1, arg2, arg3});
}

void cheats_execute_queued()
{
    while (!cheats.empty())
    {
        std::apply(replay_step_cheat, cheats.front());
        std::apply(cheats_execute, cheats.front());
        cheats.pop();
    }
}

int32_t onGoToComplete();
int32_t onHeartC();
int32_t onMagicC();
int32_t onMaxBombs();
void cheats_hit_bind(Cheat cheat)
{
	switch(cheat)
	{
		case Life:
			cheats_enqueue(Cheat::Life, game->get_maxlife());
			break;
		case Magic:
			cheats_enqueue(Cheat::Magic, game->get_maxmagic());
			break;
		case Rupies:
			cheats_enqueue(Cheat::Rupies, game->get_maxcounter(1));
			break;
		case Bombs:
			cheats_enqueue(Cheat::Bombs, game->get_maxbombs(), game->get_maxcounter(6));
			break;
		case Arrows:
			cheats_enqueue(Cheat::Arrows, game->get_maxarrows());
			break;
		case GoTo:
			onGoToComplete();
			break;
		case MaxLife:
			enter_sys_pal();
			onHeartC();
			exit_sys_pal();
			break;
		case MaxMagic:
			enter_sys_pal();
			onMagicC();
			exit_sys_pal();
			break;
		case MaxBombs:
			enter_sys_pal();
			onMaxBombs();
			exit_sys_pal();
			break;
		case PlayerData:
			enter_sys_pal();
			onCheatConsole();
			exit_sys_pal();
			break;
		default:
			cheats_enqueue(cheat);
			break;
	}
}


