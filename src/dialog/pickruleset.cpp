#include "pickruleset.h"
#include <gui/builder.h>
#include "../jwin.h"
#include "zquest.h"
#include "zq_files.h"
#include "info.h"

void call_ruleset_dlg()
{
	PickRulesetDialog(applyRuleset).show();
}

static const GUI::ListData rulesetsList
{
	{ "Modern", rulesetModern,
		"Enables all new 2.55 features including\n"
		"new Hero movement/step speed, new\n"
		"combo animations, scripting extensions,\n"
		"and other engine enhancements." },
	{ "SNES (Enhanced)", rulesetZ3,
		"As 16-bit, plus diagonal movement,\n"
		"new message strings, magic use, real\n"
		"arrows, more sounds, drowning, \n"
		"modern boomerang/item interaction." },
	{ "SNES (BS)", rulesetBSZ,
		"Adds expanded animations befitting a\n"
		"Super Famicom era game: Expanded\n"
		"enemy tiles, fast scrolling, new push-\n"
		"blocks, transition wipes, etc." },
	{ "Fixed NES", rulesetFixedNES,
		"Corrects a large number of oddities\n"
		"found in the original NES engine, \n"
		"such as bomb interactions. \n"
		"Enables all 'NES Fixes' Rules" },
	{ "Authentic NES", rulesetNES,
		"Emulates the behaviour, the quirks\n"
		"bugs, and oddities found in the NES\n"
		"game 'The Legend of Zelda'.\n"
		"All but a few rules are off.\n" }
};

PickRulesetDialog::PickRulesetDialog(std::function<void(int32_t)> setRuleset):
	setRuleset(setRuleset)
{}

std::shared_ptr<GUI::Widget> PickRulesetDialog::view()
{
	using namespace GUI::Builder;
	using namespace GUI::Props;
	
	return Window(
		title = "Pick Ruleset",
		onEnter = message::OK,
		onClose = message::CANCEL,
		Column(
			Label
			(
				maxLines = 4,
				textAlign = 1,
				text = "Please specify the ruleset template for your quest:\n"
				       "These rules affect select sets of engine features that\n"
				       "are enabled by default, that you may later toggle on/off,\n"
				       "based on the mechanics that you wish to use in your game."
			),
			Row(
				hPadding = 5_spx,
				this->rulesetChoice = RadioSet(
					hPadding = 4_spx,
					set = 0,
					onToggle = message::RULESET,
					checked = rulesetModern,
					data = rulesetsList
				),
				this->rulesetInfo = Label(noHLine = true,
					fitParent = true,
					framed = true,
					maxLines = 4,
					margins = 0_px,
					padding = 6_spx,
					text = rulesetsList.findInfo(rulesetModern)
				)
			),
			Label(
				hAlign = 0.5,
				maxLines = 2,
				textAlign = 1,
				text = "After creation, you can toggle individual Rules from\n"
					   "'Quest->Options->Rules' and 'ZScript->Quest Script Settings'"
			),
			Row(
				topPadding = 0.5_em,
				vAlign = 1.0,
				spacing = 2_em,
				Button(
					text = "OK",
					minwidth = 90_lpx,
					onClick = message::OK),
				Button(
					text = "Cancel",
					minwidth = 90_lpx,
					onClick = message::CANCEL)
			)
		)
	);
}

bool PickRulesetDialog::handleMessage(const GUI::DialogMessage<message>& msg)
{
	switch(msg.message)
	{
		case message::RULESET:
		{
			rulesetInfo->setText(rulesetsList.findInfo(msg.argument));
			return false;
		}	
		//Exiting messages
		case message::OK:
			setRuleset(rulesetChoice->getChecked());
			return true;
		case message::CANCEL:
		default:
			return true;
	}
}
