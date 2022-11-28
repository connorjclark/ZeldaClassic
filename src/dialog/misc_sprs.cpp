#include "misc_sprs.h"
#include "info.h"
#include "alert.h"
#include <gui/builder.h>
#include "../jwin.h"
#include "zquest.h"
#include "base/zsys.h"
#include "gui/use_size.h"
#include "zc_list_data.h"

using GUI::sized;

static const GUI::ListData miscSprsList
{
	{ "Falling Sprite:", sprFALL, "Shown when an enemy/item/etc falls in a pit" },
	{ "Drowning Sprite:", sprDROWN, "Shown when an enemy/item/etc falls in non-lava liquid" },
	{ "Lava Drowning Sprite:", sprLAVADROWN, "Shown when an enemy/item/etc falls in lava" },
	{ "Switch Poof:", sprSWITCHPOOF, "Shown when 'switching' objects with the poof switch style." }
};

MiscSprsDialog::MiscSprsDialog(byte* vals, size_t vals_per_tab, std::function<void(int32_t*)> setVals):
	setVals(setVals), vals_per_tab(vals_per_tab), sprs_list(GUI::ZCListData::miscsprites())
{
	for(auto q = 0; q < sfxMAX; ++q)
	{
		local_sprs[q] = vals[q];
	}
}

std::shared_ptr<GUI::Widget> MiscSprsDialog::view()
{
	using namespace GUI::Builder;
	using namespace GUI::Props;
	return Window(
		title = "Misc Sprites",
		onEnter = message::OK,
		onClose = message::CANCEL,
		Column(
			maxwidth = sized(308_px, 800_px),
			DDPanel(
				padding = 2_spx,
				values = local_sprs,
				count = vals_per_tab,
				ddlist = sprs_list,
				data = miscSprsList
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

bool MiscSprsDialog::handleMessage(const GUI::DialogMessage<message>& msg)
{
	switch(msg.message)
	{
		case message::OK:
			setVals(local_sprs);
		[[fallthrough]];
		case message::CANCEL:
		default:
			return true;
	}
}
