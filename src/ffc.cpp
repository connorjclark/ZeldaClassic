#include "base/zdefs.h"
#include "base/zsys.h"
#include "ffc.h"
#include "tiles.h"
#include "sprite.h"
#include "base/qrs.h"
#include "base/combo.h"
#include "zc/zc_ffc.h"

extern sprite_list Lwpns;

#ifdef IS_PLAYER
#include "zc/maps.h"
#include "zc/hero.h"
#include "zc/combos.h"
#include "base/mapscr.h"
#include "iter.h"

extern int16_t lensclk;
extern HeroClass Hero;
#endif

void ffcdata::clear()
{
	*this = ffcdata();
}

void ffcdata::draw(BITMAP* dest, int32_t xofs, int32_t yofs, bool overlay)
{
	if (flags&ffCHANGER) return;
	#ifdef IS_PLAYER
	if ((flags&ffLENSINVIS) && lensclk) return; //If lens is active and ffc is invis to lens, don't draw
	if ((flags&ffLENSVIS) && !lensclk) return; //If FFC does not require lens, or lens is active, draw
	
	if (switch_hooked)
	{
		switch(Hero.switchhookstyle)
		{
			default: case swPOOF:
				break;
			case swFLICKER:
			{
				if (abs(Hero.switchhookclk-33)&0b1000)
					break;
				return;
			}
			case swRISE:
				yofs -= 8-(abs(Hero.switchhookclk-32)/4);
				break;
		}
	}
	#endif
	
	if(!(flags&ffOVERLAY) == !overlay) //force cast both of these to boolean. They're both not, so same as if they weren't not.
	{
		int32_t tx = x + xofs - viewport.x;
		int32_t ty = y + yofs - viewport.y;
		
		if(flags&ffTRANS)
		{
			overcomboblocktranslucent(dest, tx, ty, data, cset, txsz, tysz,128);
		}
		else
		{
			overcomboblock(dest, tx, ty, data, cset, txsz, tysz);
		}
	}
}

bool ffcdata::setSolid(bool set) //exists so that ffcs can do special handling for whether to make something solid or not.
{
	bool actual = set && !(flags&ffCHANGER) && loaded;
	bool ret = solid_object::setSolid(actual);
	solid = set;
	return ret;
}
void ffcdata::updateSolid()
{
	if(setSolid(flags&ffSOLID))
		solid_update(false);
}

void ffcdata::solid_update(bool push)
{
#ifdef IS_PLAYER
	zfix dx = (x - old_x);
	zfix dy = (y - old_y);
	if((flags&ffPLATFORM) && Hero.on_ffc_platform(*this,true))
	{
		if(push)
			Hero.movexy(dx,dy,false,false,false);
		else
		{
			Hero.setXfix(Hero.getX()+dx);
			Hero.setYfix(Hero.getY()+dy);
		}
	}
	else if(hooked && push)
	{
		if (Lwpns.idFirst(wHookshot) > -1)
		{
			if (dx) 
				Hero.setXfix(Hero.getX() + dx);
			if (dy)
				Hero.setYfix(Hero.getY() + dy);
		}
		else
			hooked = false;
	}
#endif
	solid_object::solid_update(push);
}

void ffcdata::setLoaded(bool set)
{
	if(loaded==set) return;
	loaded = set;
	updateSolid();
}
bool ffcdata::getLoaded() const
{
	return loaded;
}

void ffcdata::doContactDamage(int32_t hdir)
{
#ifdef IS_PLAYER
	if(flags & (ffCHANGER | ffETHEREAL))
		return; //Changer or ethereal; has no type
	newcombo const& cmb = combobuf[data];
	if(data && isdamage_type(cmb.type))
	{
		int ffnum = -1;
		if(loaded)
		{
			auto ffc_handle = find_ffc([&](const ffc_handle_t& ffc_handle) {
				return this == ffc_handle.ffc;
			});
			if (ffc_handle)
			{
				ffnum = ffc_handle->id;
			}
		}
		if(ffnum > -1)
		{
			trigger_damage_combo(get_scr(screen), data, ZSD_FFC, ffnum, hdir, true);
		}
		else trigger_damage_combo(get_scr(screen), data, ZSD_NONE, 0, hdir, true);
	}
#endif
}

