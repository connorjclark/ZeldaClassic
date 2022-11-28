#ifndef ZC_DIALOG_FFCDLG_H
#define ZC_DIALOG_FFCDLG_H

#include <gui/dialog.h>
#include <gui/checkbox.h>
#include <gui/text_field.h>
#include <gui/selcombo_swatch.h>
#include <gui/label.h>
#include <gui/list_data.h>
#include <functional>
#include <string_view>

struct ffdata;
bool call_ffc_dialog(int32_t ffcombo, mapscr* scr = nullptr);
bool call_ffc_dialog(int32_t ffcombo, ffdata const& init, mapscr* scr = nullptr);

struct ffdata
{
	int32_t x, y, dx, dy, ax, ay;
	word data;
	byte cset;
	word delay;
	dword flags;
	byte link;
	byte twid : 2;
	byte fwid : 6;
	byte thei : 2;
	byte fhei : 6;
	int32_t script;
	int32_t initd[8];
	int32_t inita[2];
	
	ffdata();
	ffdata(mapscr const* scr, int32_t ind);
	void clear();
	void load(mapscr const* scr, int32_t ind);
	void save(mapscr* scr, int32_t ind);
	ffdata& operator=(ffdata const& other);
};

class FFCDialog: public GUI::Dialog<FFCDialog>
{
public:
	enum class message { OK, CANCEL, PLUSCS, MINUSCS };

	FFCDialog(mapscr* scr, int32_t ffind);
	FFCDialog(mapscr* scr, int32_t ffind, ffdata const& init);

	std::shared_ptr<GUI::Widget> view() override;
	bool handleMessage(const GUI::DialogMessage<message>& msg);

private:
	std::shared_ptr<GUI::SelComboSwatch> cmbsw;

	ffdata ffc;
	mapscr* thescr;
	int32_t ffind;
	GUI::ListData list_link, list_ffcscript;
};

#endif
