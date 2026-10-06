/**
 * @brief Side Walk: tapping one turn direction and then the other quickly
 * walks sideways toward the second one, for as long as it is held; while
 * walking sideways, a quick press the other way switches sides.
 *
 * "Quickly" is the window set from Dopo Options > Patchs (OFF, 100ms ..
 * 300ms), measured in real time with the core's microsecond clock
 * (I_RenderProfileUsec: the frontend's perf timer, or the monotonic
 * clock). It applies to the gamepad buttons bound to turning
 * (key_left/key_right, the classic layout's d-pad); the modern layout
 * already strafes with the left stick. process_gamepad_buttons
 * (gamepad_buttons.c) routes those buttons through the helpers below.
 */

/**
 * @brief Side Walk window and per direction tracking.
 *
 * @patch libretro/libretro.c 3399
 */

/**
 * @brief Window in milliseconds, 0 = off; not saved.
 */
int dopo_side_walk_ms = 200;

/**
 * @brief One turn direction: left (0) or right (1).
 */
typedef struct
{
   double pressed;   /* last press, usec */
   double released;  /* last release, usec */
   dbool  down;      /* button held */
   dbool  strafing;  /* held and walking sideways */
   dbool  strafed;   /* was walking sideways when last released */
} dopo_side_walk_turn_t;

static dopo_side_walk_turn_t dopo_side_walk_turn[2];

/**
 * @brief Direction of a turn key, or -1 for any other key.
 */
static int dopo_side_walk_direction(const int *gamekey)
{
   if (gamekey == &key_left)
      return 0;
   if (gamekey == &key_right)
      return 1;
   return -1;
}

static int dopo_side_walk_turn_key(int direction)
{
   return direction ? key_right : key_left;
}

static int dopo_side_walk_strafe_key(int direction)
{
   return direction ? key_straferight : key_strafeleft;
}

/**
 * @brief Key to post when a button goes down in game. A turn button
 * strafes when the other direction was pressed within the window, is held
 * while strafing, or stopped strafing within the window; the other
 * direction, if still held, is let go so turning and strafing don't mix.
 */
static int dopo_side_walk_press(const int *gamekey)
{
   const int d = dopo_side_walk_direction(gamekey);
   const double now = I_RenderProfileUsec();
   const double window = dopo_side_walk_ms * 1000.0;
   dopo_side_walk_turn_t *self, *other;

   if (d < 0)
      return *gamekey;

   self = &dopo_side_walk_turn[d];
   other = &dopo_side_walk_turn[!d];
   self->pressed = now;
   self->down = TRUE;
   self->strafing = FALSE;

   /* no clock (0) would make every press quick */
   if (!dopo_side_walk_ms || now <= 0.0)
      return *gamekey;

   if (!(now - other->pressed <= window ||
         (other->down && other->strafing) ||
         (!other->down && other->strafed && now - other->released <= window)))
      return *gamekey;

   if (other->down)
   {
      event_t up = {0};

      up.type = ev_keyup;
      up.data1 = other->strafing ? dopo_side_walk_strafe_key(!d)
                                 : dopo_side_walk_turn_key(!d);
      D_PostEvent(&up);
      other->strafing = FALSE;
   }

   self->strafing = TRUE;
   return dopo_side_walk_strafe_key(d);
}

/**
 * @brief Whether a turn button was walking sideways; when it was, *key
 * gets the strafe key to release.
 */
static dbool dopo_side_walk_release(const int *gamekey, int *key)
{
   const int d = dopo_side_walk_direction(gamekey);
   dopo_side_walk_turn_t *self;

   if (d < 0)
      return FALSE;

   self = &dopo_side_walk_turn[d];
   self->down = FALSE;
   self->released = I_RenderProfileUsec();
   self->strafed = self->strafing;
   if (!self->strafing)
      return FALSE;

   self->strafing = FALSE;
   *key = dopo_side_walk_strafe_key(d);
   return TRUE;
}

/* @endpatch */
