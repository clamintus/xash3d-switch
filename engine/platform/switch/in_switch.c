#include "common.h"

#if XASH_INPUT == INPUT_SWITCH
#include "input.h"
#include "joyinput.h"
#include "touch.h"
#include "client.h"
#include "in_switch.h"
#include <switch.h>

convar_t *switch_console;

typedef struct buttonmapping_s
{
	HidNpadButton btn;
	int key;
} buttonmapping_t;

typedef struct touchfinger_s
{
	float x, y, dx, dy;
	qboolean down;
} touchfinger_t;

static buttonmapping_t btn_map[15] =
{
	{ HidNpadButton_Minus, '~' },
	{ HidNpadButton_Plus, K_ESCAPE },
	{ HidNpadButton_Up, K_UPARROW },
	{ HidNpadButton_Right, K_MWHEELUP },
	{ HidNpadButton_Down, K_DOWNARROW },
	{ HidNpadButton_Left, K_MWHEELDOWN },
	{ HidNpadButton_ZL, K_MOUSE2 },
	{ HidNpadButton_ZR, K_MOUSE1 },
	{ HidNpadButton_L, K_CTRL },
	{ HidNpadButton_R, 'r' },
	{ HidNpadButton_StickL, K_SHIFT },
	{ HidNpadButton_A, K_ENTER },
	{ HidNpadButton_B, K_SPACE },
	{ HidNpadButton_X, 'f' },
	{ HidNpadButton_Y, 'e' },
};



static PadState pad;
static uint64_t btn_state;
static uint64_t old_btn_state;
static touchfinger_t touch_finger[10];

#define SWITCH_JOYSTICK_DEADZONE 1024

//https://github.com/fgsfdsfgs/vitaXash3D/blob/master/engine/platform/vita/in_vita.c
static void RescaleAnalog( int *x, int *y, int dead )
{
	//radial and scaled deadzone
	//http://www.third-helix.com/2013/04/12/doing-thumbstick-dead-zones-right.html
	float analogX = (float)*x;
	float analogY = (float)*y;
	float deadZone = (float)dead;
	float maximum = (float)JOYSTICK_MAX;
	float magnitude = sqrtf( analogX * analogX + analogY * analogY );
	if( magnitude >= deadZone )
	{
		float scalingFactor = maximum / magnitude * ( magnitude - deadZone ) / ( maximum - deadZone );
		*x = (int)( analogX * scalingFactor );
		*y = (int)( analogY * scalingFactor );
	}
	else
	{
		*x = 0;
		*y = 0;
	}
}

void Switch_IN_Init( void )
{
	switch_console = Cvar_Get( "switch_console", "0", FCVAR_ARCHIVE, "enable switch console override" );
	padConfigureInput( 1, HidNpadStyleSet_NpadStandard );
	padInitializeDefault( &pad );
	hidInitializeTouchScreen();
}

qboolean Switch_IN_ConsoleEnabled( void )
{
	return switch_console->value > 0;
}

void Switch_IN_HandleTouch( void )
{
	HidTouchScreenState touch_state = {0};
	hidGetTouchScreenStates( &touch_state, 1 );
	s32 touch_count = touch_state.count;

	const size_t finger_count = sizeof(touch_finger) / sizeof(touch_finger[1]);

	qboolean touched_down_now[finger_count];

	if( touch_count > 0 ) {
		HidTouchState *touch;

		for( int i = 0; i < touch_count; i ++ ) {
			touch = &touch_state.touches[i];
			if(touch->finger_id >= finger_count)
				continue;

			touchfinger_t *finger = &touch_finger[touch->finger_id];

			finger->x = touch->x / scr_width->value;
			finger->y = touch->y / scr_height->value;
			finger->dx = touch->diameter_x / scr_width->value;
			finger->dy = touch->diameter_y / scr_height->value;

			touched_down_now[touch->finger_id] = true;

			if (!touch_finger[touch->finger_id].down) {
				IN_TouchEvent( event_down, touch->finger_id, finger->x, finger->y, finger->dx, finger->dy );
				finger->down = true;
			} else {
				IN_TouchEvent( event_motion, touch->finger_id, finger->x, finger->y, finger->dx, finger->dy );
			}
		}

		for( int i = 0; i < finger_count; i ++ ) {
			if(touch_finger[i].down && !touched_down_now[i]) {
				touchfinger_t *finger = &touch_finger[i];
				finger->down = false;
				IN_TouchEvent( event_up, i, finger->x, finger->y, finger->dx, finger->dy );
			}
		}
	} else {
		for( int i = 0; i < finger_count; i ++ ) {
			if(touch_finger[i].down) {
				touchfinger_t *finger = &touch_finger[i];
				finger->down = false;
				IN_TouchEvent( event_up, i, finger->x, finger->y, finger->dx, finger->dy );
			}
		}
	}
}

void Switch_IN_Frame( void )
{
    padUpdate( &pad );

    btn_state = padGetButtons( &pad );

    for ( int i = 0; i < 15; ++i ) {
        if ((btn_state & btn_map[i].btn) != (old_btn_state & btn_map[i].btn)) {
            Key_Event( btn_map[i].key, !!( btn_state & btn_map[i].btn ) );
        }
    }

    old_btn_state = btn_state;

	HidAnalogStickState pos_left, pos_right;

	//Read the joysticks' position
	pos_left = padGetStickPos( &pad, 0 );
	pos_right = padGetStickPos( &pad, 1 );

	if (abs( pos_left.x ) < SWITCH_JOYSTICK_DEADZONE)
		pos_left.x = 0;
	if (abs( pos_left.y ) < SWITCH_JOYSTICK_DEADZONE)
		pos_left.y = 0;

	Joy_AxisMotionEvent( 0, 0, pos_left.x );
	Joy_AxisMotionEvent( 0, 1, -pos_left.y );

	RescaleAnalog(&pos_right.x, &pos_right.y, SWITCH_JOYSTICK_DEADZONE);

	Joy_AxisMotionEvent( 0, 2, -pos_right.x );
	Joy_AxisMotionEvent( 0, 3, pos_right.y );

	Switch_IN_HandleTouch();
}

#endif