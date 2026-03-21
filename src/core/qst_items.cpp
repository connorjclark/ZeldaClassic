#include "core/qrs.h"
#include "core/qst.h"
#include "zalleg/packfile.h"
#include "zc/ffscript.h"

extern const byte* legacy_skip_flags;
extern dword loading_tileset_flags;

static int32_t read_single_item_old(PACKFILE *f, word s_version, word index, word version, word build)
{
    byte padding, tempbyte;
    int32_t dummy;
    word dummy_word;
	
	bool should_skip = legacy_skip_flags && get_bit(legacy_skip_flags, skip_items);
	itemdata tempitem = itemdata();
	reset_itembuf(&tempitem, index);
	tempitem.name = itemsbuf.get(index).name;
	
	if ( s_version > 35 ) //expanded tiles	
	{    
		if(!p_igetl(&tempitem.tile,f))
		{
			return qe_invalid;
		}
	}
	else
	{
		if(!p_igetw(&tempitem.tile,f))
		{
			return qe_invalid;
		}
	}
	
	if(!p_getc(&tempitem.misc_flags,f))
	{
		return qe_invalid;
	}
	
	if(!p_getc(&tempitem.csets,f))
	{
		return qe_invalid;
	}
	
	if(!p_getc(&tempitem.frames,f))
	{
		return qe_invalid;
	}
	
	if(!p_getc(&tempitem.speed,f))
	{
		return qe_invalid;
	}
	
	if(!p_getc(&tempitem.delay,f))
	{
		return qe_invalid;
	}
	
	if(version < 0x193)
	{
		if(!p_getc(&padding,f))
		{
			return qe_invalid;
		}
		
		if((version < 0x192)||((version == 0x192)&&(build<186)))
		{
			if (should_skip)
				return 0;

			switch(index)
			{
			case iShield:
				tempitem.ltm=get_qr(qr_BSZELDA)?-12:10;
				break;
				
			case iMShield:
				tempitem.ltm=get_qr(qr_BSZELDA)?-6:-10;
				break;
				
			default:
				tempitem.ltm=0;
				break;
			}
			
			tempitem.count=-1;
			tempitem.flags=item_none;
			tempitem.wpn_sprites[0]=tempitem.wpn_sprites[1]=tempitem.wpn_sprites[2]=tempitem.wpn_sprites[2]=tempitem.pickup_hearts=
											tempitem.misc1=tempitem.misc2=tempitem.usesound=0;
			tempitem.type=0xFF;
			tempitem.playsound=WAV_SCALE;
			reset_itembuf(&tempitem,index);
			
			itemsbuf[index] = tempitem;
			
			return 0;
		}
	}
	
	if(!p_igetl(&tempitem.ltm,f))
	{
		return qe_invalid;
	}
	
	if(version < 0x193)
	{
		for(int32_t q=0; q<12; q++)
		{
			if(!p_getc(&padding,f))
			{
				return qe_invalid;
			}
		}
	}
	
	if(s_version>1)
	{
		if ( s_version >= 31 )
		{
			if(!p_igetl(&tempitem.type,f))
			{
				return qe_invalid;
			}    
		}
		else
		{		    
			if(!p_getc(&tempitem.type,f))
			{
				return qe_invalid;
			}
		}
		if(s_version < 16)
			if(tempitem.type == 0xFF)
				tempitem.type = itype_misc;
				
		if(!p_getc(&tempitem.level,f))
		{
			return qe_invalid;
		}
		
		if(s_version>5)
		{
			if(s_version>=31)
			{
				if(!p_igetl(&tempitem.power,f))
				{
					return qe_invalid;
				}
			}
			else
			{
				if(!p_getc(&tempitem.power,f))
				{
				return qe_invalid;
				}
			}
					
			//converted flags from 16b to 32b -Z
			if ( s_version < 41 )
			{
				if(!p_igetw(&tempitem.flags,f))
				{
					return qe_invalid;
				}
			}
			else
			{
				if(!p_igetl(&tempitem.flags,f))
				{
					return qe_invalid;
				}
			}
		}
		else
		{
			//tempitem.power = tempitem.fam_type;
			char tempchar;
			
			if(!p_getc(&tempchar,f))
			{
				return qe_invalid;
			}
			
			if (tempchar) tempitem.flags |= item_gamedata;
		}
		
		if(!p_igetw(&tempitem.script,f))
		{
			return qe_invalid;
		}
		
		if(s_version<=3)
		{
			if(tempitem.script > NUMSCRIPTITEM)
			{
				tempitem.script = 0;
			}
		}
		
		if(!p_getc(&tempitem.count,f))
		{
			return qe_invalid;
		}
		
		if(!p_igetw(&tempitem.amount,f))
		{
			return qe_invalid;
		}
		
		if(!p_igetw(&tempitem.collect_script,f))
		{
			return qe_invalid;
		}
		
		if(s_version<=3)
		{
			if(tempitem.collect_script > NUMSCRIPTITEM)
			{
				tempitem.collect_script = 0;
			}
		}
		
		if(!p_igetw(&tempitem.setmax,f))
		{
			return qe_invalid;
		}
		
		if(!p_igetw(&tempitem.max,f))
		{
			return qe_invalid;
		}
		
		if (s_version >= 66)
		{
			if(!p_igetw(&tempitem.playsound,f))
				return qe_invalid;
		}
		else
		{
			if(!p_getc(&tempbyte,f))
				return qe_invalid;
			tempitem.playsound = tempbyte;
		}
		
		for(int32_t j=0; j<8; j++)
		{
			if(!p_igetl(&tempitem.initiald[j],f))
			{
				return qe_invalid;
			}
		}
		
		for(int32_t j=0; j<2; j++)
		{
			byte temp;
			if(!p_getc(&temp,f))
			{
				return qe_invalid;
			}
		}
		
		if(s_version>4)
		{
			if(s_version>5)
			{
				int count = s_version >= 15 ? 10 : 4;
				for (int q = 0; q < count; ++q)
				{
					if(!p_getc(&tempbyte,f))
						return qe_invalid;
					tempitem.wpn_sprites[q] = tempbyte;
				}
				
				if(!p_getc(&tempitem.pickup_hearts,f))
				{
					return qe_invalid;
				}
				
				if(s_version<15)
				{
					if(!p_igetw(&dummy_word,f))
					{
						return qe_invalid;
					}
					
					tempitem.misc1=dummy_word;
					
					if(!p_igetw(&dummy_word,f))
					{
						return qe_invalid;
					}
					
					tempitem.misc2=dummy_word;
				}
				else
				{
					if(!p_igetl(&tempitem.misc1,f))
					{
						return qe_invalid;
					}
					
					if(!p_igetl(&tempitem.misc2,f))
					{
						return qe_invalid;
					}
					
					// Version 24: sh_ice -> sh_script; previously, all shields could block script weapons
					if(s_version<24)
					{
						if(tempitem.type==itype_shield)
						{
							tempitem.misc1|=sh_script;
						}
					}
				}
				
				if(s_version < 53)
				{
					byte tempbyte;
					if(!p_getc(&tempbyte,f))
					{
						return qe_invalid;
					}
					tempitem.cost_amount[0] = tempbyte;
				}
				else
				{
					for(auto q = 0; q < 2; ++q)
					{
						if(!p_igetw(&tempitem.cost_amount[q],f))
						{
							return qe_invalid;
						}
					}
				}
			}
			else
			{
				char tempchar;
				
				if(!p_getc(&tempchar,f))
				{
					return qe_invalid;
				}
				
				if (tempchar) tempitem.flags |= item_edible;
			}
			
			// June 2007: more misc. attributes
			if(s_version>=12)
			{
				if(s_version<15)
				{
					if(!p_igetw(&dummy_word,f))
					{
						return qe_invalid;
					}
					
					tempitem.misc3=dummy_word;
					
					if(!p_igetw(&dummy_word,f))
					{
						return qe_invalid;
					}
					
					tempitem.misc4=dummy_word;
				}
				else
				{
					if(!p_igetl(&tempitem.misc3,f))
					{
						return qe_invalid;
					}
					
					if(!p_igetl(&tempitem.misc4,f))
					{
						return qe_invalid;
					}
					
					if(!p_igetl(&tempitem.misc5,f))
					{
						return qe_invalid;
					}
					
					if(!p_igetl(&tempitem.misc6,f))
					{
						return qe_invalid;
					}
					
					if(!p_igetl(&tempitem.misc7,f))
					{
						return qe_invalid;
					}
					
					if(!p_igetl(&tempitem.misc8,f))
					{
						return qe_invalid;
					}
					
					if(!p_igetl(&tempitem.misc9,f))
					{
						return qe_invalid;
					}
					
					if(!p_igetl(&tempitem.misc10,f))
					{
						return qe_invalid;
					}
				}
				
				
				if (s_version >= 66)
				{
					if(!p_igetw(&tempitem.usesound,f))
						return qe_invalid;
					if(!p_igetw(&tempitem.usesound2,f))
						return qe_invalid;
				}
				else
				{
					if(!p_getc(&tempbyte,f))
						return qe_invalid;
					tempitem.usesound = tempbyte;
					if (s_version >= 49)
					{
						if(!p_getc(&tempbyte,f))
							return qe_invalid;
						tempitem.usesound2 = tempbyte;
					}
					else tempitem.usesound2 = 0;
				}
				if(s_version < 50 && tempitem.type == itype_mirror)
				{
					//Split continue/dmap warp effect/sfx, port for old
					tempitem.misc2 = tempitem.misc1;
					tempitem.usesound2 = tempitem.usesound;
				}
			}
		}
	
		if ( s_version >= 26 )  //! New itemdata vars for weapon editor. -Z
		{			// temp.useweapon, temp.usedefence, temp.weaprange, temp.weap_pattern[ITEM_MOVEMENT_PATTERNS]
			if(s_version < 63)
			{
				if(!p_getc(&tempitem.weap_data.imitate_weapon,f))
				{
					return qe_invalid;
				}
				if(!p_getc(&tempitem.weap_data.default_defense,f))
				{
					return qe_invalid;
				}
			}
			if(!p_igetl(&tempitem.weaprange,f))
			{
				return qe_invalid;
			}
			if(!p_igetl(&tempitem.weapduration,f))
			{
				return qe_invalid;
			}
			for ( int32_t q = 0; q < ITEM_MOVEMENT_PATTERNS; q++ )
			{
				if(!p_igetl(&tempitem.weap_pattern[q],f))
				{
					return qe_invalid;
				}
			}
		}
		
		if ( s_version >= 27 )  //! New itemdata vars for weapon editor. -Z
		{			// temp.useweapon, temp.usedefence, temp.weaprange, temp.weap_pattern[ITEM_MOVEMENT_PATTERNS]
			if(!p_igetl(&tempitem.duplicates,f))
			{
				return qe_invalid;
			}
			if(s_version < 63)
				for ( int32_t q = 0; q < INITIAL_D; q++ )
					if(!p_igetl(&tempitem.weap_data.initd[q],f))
						return qe_invalid;
			for ( int32_t q = 0; q < 2; q++ )
			{
				byte temp;
				if(!p_getc(&temp,f))
				{
					return qe_invalid;
				}
			}
			
			if(!p_getc(&tempitem.drawlayer,f))
			{
				return qe_invalid;
			}
			
			
			if(!p_igetl(&tempitem.hxofs,f))
			{
				return qe_invalid;
			}
			if(!p_igetl(&tempitem.hyofs,f))
			{
				return qe_invalid;
			}
			if(!p_igetl(&tempitem.hxsz,f))
			{
				return qe_invalid;
			}
			if(!p_igetl(&tempitem.hysz,f))
			{
				return qe_invalid;
			}
			if(!p_igetl(&tempitem.hzsz,f))
			{
				return qe_invalid;
			}
			if(!p_igetl(&tempitem.xofs,f))
			{
				return qe_invalid;
			}
			if(!p_igetl(&tempitem.yofs,f))
			{
				return qe_invalid;
			}
			if(s_version < 63)
			{
				if(!p_igetl(&tempitem.weap_data.hxofs,f))
					return qe_invalid;
				if(!p_igetl(&tempitem.weap_data.hyofs,f))
					return qe_invalid;
				if(!p_igetl(&tempitem.weap_data.hxsz,f))
					return qe_invalid;
				if(!p_igetl(&tempitem.weap_data.hysz,f))
					return qe_invalid;
				if(!p_igetl(&tempitem.weap_data.hzsz,f))
					return qe_invalid;
				if(!p_igetl(&tempitem.weap_data.xofs,f))
					return qe_invalid;
				if(!p_igetl(&tempitem.weap_data.yofs,f))
					return qe_invalid;
			}
			if(s_version < 63)
				if(!p_igetw(&tempitem.weap_data.script,f))
					return qe_invalid;
			if(!p_igetl(&tempitem.wpnsprite,f))
			{
				return qe_invalid;
			}
			auto num_cost_tmr = (s_version > 52 ? 2 : 1);
			for(auto q = 0; q < num_cost_tmr; ++q)
			{
				if(!p_igetl(&tempitem.magiccosttimer[q],f))
				{
					return qe_invalid;
				}
			}
			for(auto q = num_cost_tmr; q < 2; ++q)
				tempitem.magiccosttimer[q] = 0;
		}
		if ( s_version >= 28 )  //! New itemdata vars for weapon editor. -Z
		{
			//Item Size FLags, TileWidth, TileHeight
			if(!p_igetl(&tempitem.overrideFLAGS,f))
			{
				return qe_invalid;
			}
			if(!p_igetl(&tempitem.tilew,f))
			{
				return qe_invalid;
			}
			if(!p_igetl(&tempitem.tileh,f))
			{
				return qe_invalid;
			}
		}
		if ( s_version >= 29 && s_version < 63)  //! More new vars. 
		{
			if(!p_igetl(&tempitem.weap_data.override_flags,f))
				return qe_invalid;
			if(!p_igetl(&tempitem.weap_data.tilew,f))
				return qe_invalid;
			if(!p_igetl(&tempitem.weap_data.tileh,f))
				return qe_invalid;
		}
		if ( s_version >= 30 )  //! More new vars. 
		{
			//Pickup Type
			if(!p_igetl(&tempitem.pickup,f))
			{
				return qe_invalid;
			}
		}
		if ( s_version >= 32 )  //! More new vars. 
		{
			//Pickup Type
			if(!p_igetw(&tempitem.pstring,f))
			{
				return qe_invalid;
			}
		}
		if ( s_version >= 33 )  //! More new vars. 
		{
			//Pickup Type
			if(!p_igetw(&tempitem.pickup_string_flags,f))
			{
				return qe_invalid;
			}
		}
		if ( s_version >= 34 )  //! cost counter
		{
			if(s_version < 53)
			{
				if(!p_getc(&tempitem.cost_counter[0],f))
				{
					return qe_invalid;
				}
			}
			else
			{
				for(auto q = 0; q < 2; ++q)
				{
					if(!p_getc(&tempitem.cost_counter[q],f))
					{
						return qe_invalid;
					}
				}
			}
		}
		if ( s_version >= 44 )  //! sprite scripts
		{
			for ( int32_t q = 0; q < 8; q++ )
			{
				for ( int32_t w = 0; w < 65; w++ )
				{
					if(!p_getc(&(tempitem.initD_label[q][w]),f))
					{
						return qe_invalid;
					} 
				}
				if(s_version < 63)
					for ( int32_t w = 0; w < 65; w++ )
						if(!p_getc(&padding,f))
							return qe_invalid;
				for ( int32_t w = 0; w < 65; w++ )
				{
					if(!p_getc(&(tempitem.sprite_initD_label[q][w]),f))
					{
						return qe_invalid;
					} 
				}
				if(!p_igetl(&(tempitem.sprite_initiald[q]),f))
				{
					return qe_invalid;
				}
				
			}
			for ( int32_t q = 0; q < 2; q++ )
			{
				byte temp;
				if(!p_getc(&temp,f))
				{
					return qe_invalid;
				}
			}
			//Pickup Type
			if(!p_igetw(&tempitem.sprite_script,f))
			{
				return qe_invalid;
			}
		}
		if ( s_version >= 48 )  //! pickup flags
		{
			if(!p_getc(&(tempitem.pickupflag),f))
			{
				return qe_invalid;
			}
		}
		if ( s_version >= 57 )
		{
			if(!p_getcstr(&tempitem.display_name,f))
				return qe_invalid;
		}
	}
	else
	{
		tempitem.count=-1;
		tempitem.type=itype_misc;
		tempitem.flags=item_none;
		tempitem.wpn_sprites[0]=tempitem.wpn_sprites[1]=tempitem.wpn_sprites[2]=tempitem.pickup_hearts=tempitem.misc1=tempitem.misc2=tempitem.usesound=0;
		tempitem.playsound=WAV_SCALE;
		reset_itembuf(&tempitem,index);
	}
	
	if(s_version >= 58 && s_version < 63)
	{
		for(int q = 0; q < WPNSPR_MAX; ++q)
		{
			if(!p_getc(&tempbyte,f))
				return qe_invalid;
			tempitem.weap_data.burnsprs[q] = tempbyte;
			if(s_version >= 59)
				if(!p_getc(&tempitem.weap_data.light_rads[q],f))
					return qe_invalid;
		}
	}
	
	if ( s_version >= 60 )
	{
		if ( s_version >= 65 )
		{
			if(!p_igetw(&tempitem.pickup_litems,f))
				return qe_invalid;
		}
		else
		{
			if(!p_getc(&padding,f))
				return qe_invalid;
			tempitem.pickup_litems = word(padding);
		}
		if(!p_igetw(&tempitem.pickup_litem_level,f))
			return qe_invalid;
	}
	
	if ( s_version >= 62 )
	{
		if (!p_igetl(&tempitem.moveflags, f))
			return qe_invalid;
		if(s_version < 63)
			if (!p_igetl(&tempitem.weap_data.moveflags, f))
				return qe_invalid;
	}
	else
	{
		tempitem.moveflags = (move_obeys_grav | move_can_pitfall);
		switch(tempitem.type)
		{
			case itype_divinefire:
				if(!(tempitem.flags & item_flag3))
					break;
			[[fallthrough]];
			case itype_bomb: case itype_sbomb:
			case itype_bait: case itype_liftglove:
			case itype_candle: case itype_book:
				tempitem.weap_data.moveflags = (move_obeys_grav | move_can_pitfall);
				break;
			default:
				tempitem.weap_data.moveflags = move_none;
				break;
		}
	}
	
	if(s_version >= 63)
	{
		if(auto ret = read_weap_data(tempitem.weap_data, f))
			return ret;
	}
	else
	{
		SETFLAG(tempitem.weap_data.wflags, WFLAG_UPDATE_IGNITE_SPRITE, tempitem.flags & item_burning_sprites);
		switch(tempitem.type)
		{
			case itype_liftglove:
				tempitem.weap_data.wflags = WFLAG_BREAK_WHEN_LANDING;
				break;
			case itype_bomb: case itype_sbomb:
				// Moving these over and removing them from itemdata
				if(tempitem.flags & item_flag3)
					tempitem.weap_data.wflags |= WFLAG_STOP_WHEN_LANDING;
				if(tempitem.flags & item_flag5)
					tempitem.weap_data.wflags |= WFLAG_STOP_WHEN_HIT_SOLID;
				tempitem.flags &= ~(item_flag3|item_flag5);
				if(tempitem.misc4)
				{
					tempitem.weap_data.lift_level = tempitem.misc4;
					tempitem.weap_data.lift_time = tempitem.misc5;
					tempitem.weap_data.lift_height = tempitem.misc6;
					tempitem.misc4 = 0;
					tempitem.misc5 = 0;
					tempitem.misc6 = 0;
				}
				break;
		}
	}
	
	if (s_version >= 64)
	{
		if (!p_igetl(&tempitem.cooldown, f))
			return qe_invalid;
	}
	
	if (!should_skip)
	{
		if(loading_tileset_flags & TILESET_CLEARSCRIPTS)
		{
			tempitem.script = 0;
			tempitem.weap_data.script = 0;
			for(int q = 0; q < 8; ++q)
			{
				tempitem.initiald[q] = 0;
				tempitem.weap_data.initd[q] = 0;
			}
		}
		itemsbuf[index] = tempitem;
	}
	return 0;
}

static int32_t read_items_old(PACKFILE *f, word s_version, word version, word build)
{
	bool should_skip = legacy_skip_flags && get_bit(legacy_skip_flags, skip_items);

    word items_to_read = 256;
    
    if(version < 0x186)
    {
        items_to_read=64;
    }
    
    if(version > 0x192)
    {
        items_to_read=0;
		
        //finally...  section data
        if(!p_igetw(&items_to_read,f))
        {
            return qe_invalid;
        }

        if (items_to_read > MAXITEMS)
        {
            return qe_invalid;
        }
    }
    
    
	if (!should_skip)
	{
		itemsbuf.clear();
		// default any items that might be needed for hardcoded behavior,
		// which will be missed in the reading code (>= items_to_read).
		itemsbuf.reserve(zc_max(items_to_read, iLast));
		for (int q = items_to_read; q < iLast; ++q)
		{ // likely only runs version < 0x186?
			itemdata& id = itemsbuf[q];
			reset_itembuf(&id, q);
		}
	}
    if(s_version>1)
    {
        for(int32_t i=0; i<items_to_read; i++)
        {
            char tempname[64];
            
            if(!pfread(tempname, 64, f))
                return qe_invalid;
            
			tempname[63] = 0;
			if (!should_skip)
				itemsbuf[i].name = tempname;
        }
    }
    else if (!should_skip)
    {
		for(int32_t i=0; i<256; i++)
			reset_itemname(i);
    }
    
    for (int32_t i=0; i<items_to_read; i++)
		if (auto ret = read_single_item_old(f, s_version, i, version, build))
			return ret;

	if (!should_skip)
		for(word i = 0; i < itemsbuf.capacity(); ++i)
			update_old_item(s_version, i, version);
	
	return 0;
}

int32_t read_single_item(PACKFILE *f, word s_version, word index, word version, word build)
{
	if (s_version < 67)
		return read_single_item_old(f, s_version, index, version, build);
	static itemdata _nil_item;
	
	byte tempbyte;
	bool should_skip = legacy_skip_flags && get_bit(legacy_skip_flags, skip_items);
	itemdata& item_ref = should_skip ? _nil_item : itemsbuf[index];
	item_ref = itemdata();
	reset_itembuf(&item_ref, index);
	
	if (!p_getcstr(&item_ref.name, f))
		return qe_invalid;
	
	if(!p_igetl(&item_ref.tile,f))
		return qe_invalid;
	
	if(!p_getc(&item_ref.misc_flags,f))
		return qe_invalid;
	
	if(!p_getc(&item_ref.csets,f))
		return qe_invalid;
	
	if(!p_getc(&item_ref.frames,f))
		return qe_invalid;
	
	if(!p_getc(&item_ref.speed,f))
		return qe_invalid;
	
	if(!p_getc(&item_ref.delay,f))
		return qe_invalid;
	
	if(!p_igetl(&item_ref.ltm,f))
		return qe_invalid;
	
	if(!p_igetl(&item_ref.type,f))
		return qe_invalid;
	
	if(!p_getc(&item_ref.level,f))
		return qe_invalid;
	
	if(!p_igetl(&item_ref.power,f))
		return qe_invalid;
	
	if(!p_igetl(&item_ref.flags,f))
		return qe_invalid;
	
	if(!p_igetw(&item_ref.script,f))
		return qe_invalid;
	
	if(!p_getc(&item_ref.count,f))
		return qe_invalid;
	
	if(!p_igetw(&item_ref.amount,f))
		return qe_invalid;
	
	if(!p_igetw(&item_ref.collect_script,f))
		return qe_invalid;
	
	if(!p_igetw(&item_ref.setmax,f))
		return qe_invalid;
	
	if(!p_igetw(&item_ref.max,f))
		return qe_invalid;
	
	if(!p_igetw(&item_ref.playsound,f))
		return qe_invalid;
	
	for (size_t q = 0; q < 8; ++q)
		if(!p_igetl(&item_ref.initiald[q], f))
			return qe_invalid;
	
	if (s_version < 68)
	{
		for (int q = 0; q < 10; ++q)
		{
			if(!p_getc(&tempbyte,f))
				return qe_invalid;
			item_ref.wpn_sprites[q] = tempbyte;
		}
	}
	else
	{
		for (int q = 0; q < 10; ++q)
			if(!p_igetw(&item_ref.wpn_sprites[q],f))
				return qe_invalid;
	}
	
	if(!p_getc(&item_ref.pickup_hearts,f))
		return qe_invalid;
	
	if(!p_igetl(&item_ref.misc1,f))
		return qe_invalid;
	
	if(!p_igetl(&item_ref.misc2,f))
		return qe_invalid;
	
	for(size_t q = 0; q < 2; ++q)
		if(!p_igetw(&item_ref.cost_amount[q],f))
			return qe_invalid;
	
	if(!p_igetl(&item_ref.misc3,f))
		return qe_invalid;
	
	if(!p_igetl(&item_ref.misc4,f))
		return qe_invalid;
	
	if(!p_igetl(&item_ref.misc5,f))
		return qe_invalid;
	
	if(!p_igetl(&item_ref.misc6,f))
		return qe_invalid;
	
	if(!p_igetl(&item_ref.misc7,f))
		return qe_invalid;
	
	if(!p_igetl(&item_ref.misc8,f))
		return qe_invalid;
	
	if(!p_igetl(&item_ref.misc9,f))
		return qe_invalid;
	
	if(!p_igetl(&item_ref.misc10,f))
		return qe_invalid;
	
	if(!p_igetw(&item_ref.usesound,f))
		return qe_invalid;
	if(!p_igetw(&item_ref.usesound2,f))
		return qe_invalid;
	
	if(!p_igetl(&item_ref.weaprange,f))
		return qe_invalid;
	if(!p_igetl(&item_ref.weapduration,f))
		return qe_invalid;
	for (size_t q = 0; q < ITEM_MOVEMENT_PATTERNS; ++q)
		if(!p_igetl(&item_ref.weap_pattern[q],f))
			return qe_invalid;
	
	if(!p_igetl(&item_ref.duplicates,f))
		return qe_invalid;
	if(!p_getc(&item_ref.drawlayer,f))
		return qe_invalid;
	
	if(!p_igetl(&item_ref.hxofs,f))
		return qe_invalid;
	if(!p_igetl(&item_ref.hyofs,f))
		return qe_invalid;
	if(!p_igetl(&item_ref.hxsz,f))
		return qe_invalid;
	if(!p_igetl(&item_ref.hysz,f))
		return qe_invalid;
	if(!p_igetl(&item_ref.hzsz,f))
		return qe_invalid;
	if(!p_igetl(&item_ref.xofs,f))
		return qe_invalid;
	if(!p_igetl(&item_ref.yofs,f))
		return qe_invalid;
	if(!p_igetl(&item_ref.wpnsprite,f))
		return qe_invalid;
	for(size_t q = 0; q < 2; ++q)
		if(!p_igetl(&item_ref.magiccosttimer[q],f))
			return qe_invalid;
	
	if(!p_igetl(&item_ref.overrideFLAGS,f))
		return qe_invalid;
	if(!p_igetl(&item_ref.tilew,f))
		return qe_invalid;
	if(!p_igetl(&item_ref.tileh,f))
		return qe_invalid;
	if(!p_igetl(&item_ref.pickup,f))
		return qe_invalid;
	if(!p_igetw(&item_ref.pstring,f))
		return qe_invalid;
	if(!p_igetw(&item_ref.pickup_string_flags,f))
		return qe_invalid;
	for(size_t q = 0; q < 2; ++q)
		if(!p_getc(&item_ref.cost_counter[q],f))
			return qe_invalid;
		
	for ( int32_t q = 0; q < 8; q++ )
	{
		for ( int32_t w = 0; w < 65; w++ )
			if(!p_getc(&(item_ref.initD_label[q][w]),f))
				return qe_invalid;
		for ( int32_t w = 0; w < 65; w++ )
			if(!p_getc(&(item_ref.sprite_initD_label[q][w]),f))
				return qe_invalid;
		if(!p_igetl(&(item_ref.sprite_initiald[q]),f))
			return qe_invalid;
	}
	if(!p_igetw(&item_ref.sprite_script,f))
		return qe_invalid;
	
	if(!p_getc(&(item_ref.pickupflag),f))
		return qe_invalid;
	if(!p_getcstr(&item_ref.display_name,f))
		return qe_invalid;
	
	if(!p_igetw(&item_ref.pickup_litems,f))
		return qe_invalid;
	if(!p_igetw(&item_ref.pickup_litem_level,f))
		return qe_invalid;
	
	if (!p_igetl(&item_ref.moveflags, f))
		return qe_invalid;
	
	if(auto ret = read_weap_data(item_ref.weap_data, f))
		return ret;
	
	if (!p_igetl(&item_ref.cooldown, f))
		return qe_invalid;

	if (!should_skip)
	{
		if(loading_tileset_flags & TILESET_CLEARSCRIPTS)
		{
			item_ref.script = 0;
			item_ref.weap_data.script = 0;
			for(int q = 0; q < 8; ++q)
			{
				item_ref.initiald[q] = 0;
				item_ref.weap_data.initd[q] = 0;
			}
		}
	}
	return 0;
}

int32_t readitems(PACKFILE *f, word version, word build)
{
	if (version <= 0x192)
		return read_items_old(f, 0, version, build);
	
	word s_version;
	int32_t dummy;
	if(!p_igetw(&s_version,f))
		return qe_invalid;
	if (s_version > V_ITEMS)
		return qe_version;
	
	FFCore.quest_format[vItems] = s_version;
		
	if(!read_deprecated_section_cversion(f))
		return qe_invalid;
	
	//section size
	if(!p_igetl(&dummy,f))
		return qe_invalid;
	
	if (s_version < 67)
		return read_items_old(f, s_version, version, build);
	
	word items_to_read = 0;
	if (!p_igetw(&items_to_read, f))
		return qe_invalid;
	if (items_to_read > MAXITEMS)
		return qe_invalid;
	itemsbuf.clear();
	itemsbuf.reserve(items_to_read);

	for (word q = 0; q < items_to_read; ++q)
		if (auto ret = read_single_item(f, s_version, q, version, build))
			return ret;
	for (word q = 0; q < items_to_read; ++q)
		update_old_item(s_version, q, version);
	
	itemsbuf.normalize();
	
	return 0;
}
