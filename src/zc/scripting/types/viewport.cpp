#include "zc/scripting/types/viewport.h"

#include "base/check.h"
#include "components/zasm/defines.h"
#include "core/zdefs.h"
#include "zc/ffscript.h"
#include "zc/maps.h"
#include "zc/scripting/common.h"

extern refInfo *ri;
extern int32_t sarg1;
extern int32_t sarg2;
extern int32_t sarg3;

int32_t viewport_get_register(int32_t reg)
{
	int32_t ret = 0;

	switch (reg)
	{
		case VIEWPORT_HEIGHT:
		{
			ret = viewport.h * 10000;
		}
		break;
		case VIEWPORT_MODE:
		{
			ret = (int)viewport_mode;
		}
		break;
		case VIEWPORT_TARGET:
		{
			ret = get_viewport_sprite()->uid;
		}
		break;
		case VIEWPORT_WIDTH:
		{
			ret = viewport.w * 10000;
		}
		break;
		case VIEWPORT_DEADZONE_WIDTH:
		{
			ret = get_viewport_deadzone_width() * 10000;
		}
		break;
		case VIEWPORT_DEADZONE_HEIGHT:
		{
			ret = get_viewport_deadzone_height() * 10000;
		}
		break;
		case VIEWPORT_LOOKAHEAD_X:
		{
			ret = get_viewport_lookahead_x() * 10000;
		}
		break;
		case VIEWPORT_LOOKAHEAD_Y:
		{
			ret = get_viewport_lookahead_y() * 10000;
		}
		break;
		case VIEWPORT_LOOKAHEAD_SPEED:
		{
			ret = get_viewport_lookahead_speed().getZLong();
		}
		break;
		case VIEWPORT_RECENTER_SPEED:
		{
			ret = get_viewport_recenter_speed().getZLong();
		}
		break;
		case VIEWPORT_RECENTER_DELAY:
		{
			ret = get_viewport_recenter_delay() * 10000;
		}
		break;
		case VIEWPORT_X:
		{
			ret = viewport.x * 10000;
		}
		break;
		case VIEWPORT_Y:
		{
			ret = viewport.y * 10000;
		}
		break;

		default:
			NOTREACHED();
	}

	return ret;
}

void viewport_set_register(int32_t reg, int32_t value)
{
	switch (reg)
	{
		case VIEWPORT_HEIGHT:
		{
			int val = value / 10000;
			if (BC::checkBounds(val, 0, 232) != SH::_NoError)
				break;

			viewport.h = val;
		}
		break;
		case VIEWPORT_MODE:
		{
			int val = value;
			if (BC::checkBounds(val, (int)ViewportMode::First, (int)ViewportMode::Last) != SH::_NoError)
			{
				break;
			}

			viewport_mode = (ViewportMode)val;
		}
		break;
		case VIEWPORT_TARGET:
		{
			if (auto s = ResolveBaseSprite(value))
			{
				set_viewport_sprite(s);
				reset_viewport_follow();
				update_viewport();
			}
		}
		break;
		case VIEWPORT_WIDTH:
		{
			int val = value / 10000;
			if (BC::checkBounds(val, 0, 256) != SH::_NoError)
				break;

			viewport.w = val;
		}
		break;
		case VIEWPORT_DEADZONE_WIDTH:
		{
			int val = value / 10000;
			if (BC::checkBounds(val, 0, 2*VIEWPORT_FOLLOW_MAX_OFFSET_X) != SH::_NoError)
				break;

			set_viewport_deadzone_width(val);
			update_viewport();
		}
		break;
		case VIEWPORT_DEADZONE_HEIGHT:
		{
			int val = value / 10000;
			if (BC::checkBounds(val, 0, 2*VIEWPORT_FOLLOW_MAX_OFFSET_Y) != SH::_NoError)
				break;

			set_viewport_deadzone_height(val);
			update_viewport();
		}
		break;
		case VIEWPORT_LOOKAHEAD_X:
		{
			int val = value / 10000;
			if (BC::checkBounds(val, INT8_MIN, INT8_MAX) != SH::_NoError)
				break;

			set_viewport_lookahead_x(val);
		}
		break;
		case VIEWPORT_LOOKAHEAD_Y:
		{
			int val = value / 10000;
			if (BC::checkBounds(val, INT8_MIN, INT8_MAX) != SH::_NoError)
				break;

			set_viewport_lookahead_y(val);
		}
		break;
		case VIEWPORT_LOOKAHEAD_SPEED:
		{
			// Fractional, so no truncation to int (a negative value is an error rather than
			// silently becoming 0).
			zfix val = zslongToFix(value);
			if (val < 0 || val > VIEWPORT_FOLLOW_MAX_SPEED)
			{
				_scripting_log_error_with_context("Invalid value: {} - must be >= 0 and <= {}", val, VIEWPORT_FOLLOW_MAX_SPEED);
				break;
			}

			set_viewport_lookahead_speed(val);
		}
		break;
		case VIEWPORT_RECENTER_SPEED:
		{
			zfix val = zslongToFix(value);
			if (val < 0 || val > VIEWPORT_FOLLOW_MAX_SPEED)
			{
				_scripting_log_error_with_context("Invalid value: {} - must be >= 0 and <= {}", val, VIEWPORT_FOLLOW_MAX_SPEED);
				break;
			}

			set_viewport_recenter_speed(val);
		}
		break;
		case VIEWPORT_RECENTER_DELAY:
		{
			int val = value / 10000;
			if (BC::checkBounds(val, 0, 65535) != SH::_NoError)
				break;

			set_viewport_recenter_delay(val);
		}
		break;
		case VIEWPORT_X:
		{
			viewport.x = value / 10000;
		}
		break;
		case VIEWPORT_Y:
		{
			viewport.y = value / 10000;
		}
		break;

		default:
			NOTREACHED();
	}
}

std::optional<int32_t> viewport_run_command(word command)
{
	switch (command)
	{
		case VIEWPORT_RESET_FOLLOW_SETTINGS:
			reset_viewport_follow_settings();
			update_viewport();
			break;

		default: return std::nullopt;
	}

	return RUNSCRIPT_OK;
}
