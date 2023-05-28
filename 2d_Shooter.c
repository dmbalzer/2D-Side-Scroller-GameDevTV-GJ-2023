/*********************************************************************************************
*	Includes
**********************************************************************************************/
#include <raylib.h>
#include <raymath.h>
#include <stdio.h>
#include <stdlib.h>

/*********************************************************************************************
*	Defines 
**********************************************************************************************/
#define WINDOW_W 1000
#define WINDOW_H 600
#define TRUE 1
#define FALSE 0
#define FRAME_TARGET 60
#define PLAYER_SPEED 300
#define PLAYER_RELOAD 16
#define ENEMY_SPEED 100
#define ENEMY_RELOAD 48
#define BULLET_SPEED 400
#define ENEMY_BULLET_SPEED 250
#define ENEMY_DY 80

/*********************************************************************************************
*	Enumeration Definitions
**********************************************************************************************/
typedef enum _state State;
enum _state {
	START,
	PLAY,
	GAME_OVER,
};

/*********************************************************************************************
*	Struct Definitions
**********************************************************************************************/
typedef struct _game Game;
typedef struct _entity Entity;
typedef struct _entity_list EntityList;

struct _game {
	State state;
	int left;
	int right;
	int up;
	int down;
	int fire;
	int enter;
	int esc;
};
struct _entity {
	float x;
	float y;
	float w;
	float h;
	float dx;
	float dy;
	int health;
	int reload;
	int enemy;
	Color color;
	Entity *next;
};
struct _entity_list {
	Entity head;
	Entity* tail;
};

/*********************************************************************************************
*	Global Variables
**********************************************************************************************/
Game game = { 0 };
int running = 0;
float delta_time = 0.0f;
float difficulty = 1.0f;
int difficulty_delay = 150;
int enemy_wave_difficulty_delay = 20;
int difficulty_timer = 0;
int enemy_spawn_delay = 256;
int enemy_spawn_timer = 0;
int score = 0;
int wave = 0;
int enemy_wave = 0;
int waves_survived = 0;
int wave_cooldown = 0;
int egg = 0;
int retries = 0;
Entity* player = NULL;
EntityList enemyList = { 0 };
EntityList bulletList = { 0 };
Music soundtrack = { 0 };
Sound laserSound;

/*********************************************************************************************
*	Function Prototypes
**********************************************************************************************/
void clean_lists(void);

/*********************************************************************************************
*	Setup Functions
**********************************************************************************************/
void initialize_game(void)
{
	InitWindow(WINDOW_W, WINDOW_H, "Retro Shooter");
	SetTargetFPS(60);
	InitAudioDevice();
	soundtrack = LoadMusicStream("resources/Crowander - Space Fun.mp3");
	laserSound = LoadSound("resources/Retro Gun Laser SingleShot 01.wav");
}
int setup_game(void)
{
	/* Initialize Lists */
	clean_lists();

	/* Difficulty */
	difficulty = 1.0f;
	difficulty_timer = difficulty_delay;
	wave = 0;
	enemy_wave = 0;
	wave_cooldown = 0;

	/* Score */
	score = 0;
	waves_survived = 0;

	/* Retries for a fun message! */
	retries++;

	/* Initialize Player */
	player = (Entity*)calloc(1, sizeof(Entity));
	if (player != NULL)
	{
		player->x = 100;
		player->y = 100;
		player->color = BLUE;
		player->w = 32;
		player->h = 32;
		player->health = 1;
	}
	else
	{
		fprintf(stderr, "Error initializing player.\n");
		return FALSE;
	}

	soundtrack.looping = TRUE;
	PlayMusicStream(soundtrack);

	return TRUE;
}

/*********************************************************************************************
*	Input Functions
**********************************************************************************************/
void process_input(void) {
	
	game.up =		IsKeyDown(KEY_UP)	? 1 : 0;
	game.down =		IsKeyDown(KEY_DOWN)	? 1 : 0;
	game.left =		IsKeyDown(KEY_LEFT)	? 1 : 0;
	game.right =	IsKeyDown(KEY_RIGHT)	? 1 : 0;
	game.fire =		IsKeyDown(KEY_Z)	? 1 : 0;
	game.enter = 	IsKeyDown(KEY_ENTER)	? 1 : 0;
	game.esc = 		IsKeyDown(KEY_ESCAPE)	? 1 : 0;
	/* egg */
	if ( IsKeyPressed(KEY_TWO) ) { 
		egg = !egg;
		if ( egg ) player->color = WHITE;
		else player->color = BLUE;
	}
}

/*********************************************************************************************
*	Game Functions
**********************************************************************************************/
void fire_bullet(Entity *e)
{
	Entity* b = (Entity*)calloc(1, sizeof(Entity));
	
	int super = (wave || enemy_wave || egg); /* YES I KNOW THIS IS JENKY! */

	if (b != NULL)
	{
		b->x = e->enemy ? e->x : e->x + e->w;
		b->dx = e->enemy ? -ENEMY_BULLET_SPEED : ( super ? BULLET_SPEED * 2 : BULLET_SPEED);
		b->w = e->enemy ? 8 : ( super ? 16 : 8 );
		b->h = e->enemy ? 8 : ( super ? 16 : 8 );
		b->y = e->y + e->h / 2 - b->h / 2;
		b->color = e->enemy ? RED : ( super ? YELLOW : GREEN );
		b->health = 1;
		b->enemy = e->enemy;
		bulletList.tail->next = b;
		bulletList.tail = b;
		if ( !b->enemy ) PlaySound(laserSound);
	}
}
void bullet_hit(Entity* b)
{
	Entity* e = NULL;
	if (b->enemy)
	{
		/* Check if enemy bullet hits player */
		if (CheckCollisionRecs((Rectangle) { b->x, b->y, b->w, b->h }, (Rectangle) { player->x, player->y, player->w, player->h }))
		{
			b->health--;
			player->health--;
			if (egg) player->health = 1;
		}
	}

	/* Check if player bullet hits enemy */
	else /* Only player bullets can kill enemies */
	{
		for (e = enemyList.head.next; e != NULL; e = e->next)
		{
			if (CheckCollisionRecs((Rectangle){ b->x, b->y, b->w, b->h }, (Rectangle){ e->x, e->y, e->w, e->h }))
			{
				b->health--;
				e->health--;
				score += 10 / difficulty; /* Difficulty helps out the player score */
			}
		}
	}

	/* Bullets can kill other side bullets */
	Entity* b_other = NULL;
	for ( b_other = bulletList.head.next; b_other != NULL; b_other = b_other->next )
	{
		if ( b_other->enemy == b->enemy ) continue;
		if (CheckCollisionRecs((Rectangle) { b->x, b->y, b->w, b->h }, (Rectangle) { b_other->x, b_other->y, b_other->w, b_other->h }))
		{
			--b->health;
			--b_other->health;
		}
	}
}

void enemy_hit(void)
{
	Entity* e;
	for (e = enemyList.head.next; e != NULL; e = e->next)
	{
		if ( CheckCollisionRecs((Rectangle){ player->x, player->y, player->w, player->h }, (Rectangle){ e->x, e->y, e->w, e->h }))
		{
			if (!egg) player->health = 0;
			return;
		}
	}
}

void spawn_enemy(void)
{
	if (--enemy_spawn_timer <= 0)
	{
		enemy_spawn_timer = enemy_spawn_delay * difficulty; /* Difficulty Applied, reduces delay between spawns */
		Entity* e = (Entity*)calloc(1, sizeof(Entity));
		if (e != NULL)
		{
			e->dx = -ENEMY_SPEED;
			e->dy = (rand() % ENEMY_DY) - (ENEMY_DY / 2);
			e->w = 32;
			e->h = 32;
			e->health = 1;
			e->color = GRAY;
			e->x = WINDOW_W + e->w * 2; /* Ensure players bullets can't kill of screen enemies */
			e->enemy = 1;

			/* TODO adjust spawn height to be within window. */
			int clamp = WINDOW_H - e->h;
			e->y = rand() % clamp;

			enemyList.tail->next = e;
			enemyList.tail = e;
		}
	}
}

/*********************************************************************************************
*	Update Functions
**********************************************************************************************/
void update_player(void)
{
	float spd = egg ? (float)PLAYER_SPEED * 1.5 : (float)PLAYER_SPEED;
	Vector2 vel = (Vector2){ game.right - game.left, game.down - game.up };
	vel = Vector2Normalize(vel);
	vel = Vector2Scale(vel, (float)(spd));
	vel = Vector2Scale(vel, delta_time);

	/* Keep player in the window */
	if ( player->x + vel.x < 0 ) { player->x = 0; vel.x = 0; }
	if ( player->x + vel.x > WINDOW_W - player->w ) { player->x = WINDOW_W - player->w; vel.x = 0; }
	if ( player->y + vel.y < 0 ) { player->y = 0; vel.y = 0; }
	if ( player->y + vel.y > WINDOW_H - player->h ) { player->y = WINDOW_H - player->h; vel.y = 0; }

	player->x += vel.x;
	player->y += vel.y;

	/* Check for collisions with an enemy */
	enemy_hit();

	if (game.fire && --player->reload <= 0)
	{
		fire_bullet(player);

		if (wave || enemy_wave || egg) player->reload = PLAYER_RELOAD / 4;
		else player->reload = PLAYER_RELOAD;
	}

	if (player->health <= 0) game.state = GAME_OVER;
}
void update_enemies(void)
{
	Entity* e;
	Entity* prev = &enemyList.head;
	for (e = enemyList.head.next; e != NULL; e = e->next)
	{
		e->x += e->dx * delta_time;
		e->y += e->dy * delta_time;

		/* Bounce enemies off upper and lower screen */
		if ( e->y < 0 ) e->dy *= -1;
		if ( e->y > WINDOW_H - e->h ) e->dy *= -1;

		/* Fire Bullets */
		if (--e->reload <= 0)
		{
			fire_bullet(e);
			e->reload = ENEMY_RELOAD + rand() % ENEMY_RELOAD;
		}

		/* cleanup enemies */
		if (e->x < 0 - e->w || e->health <= 0)
		{
			if (e == enemyList.tail)
			{
				enemyList.tail = prev;
			}
			prev->next = e->next;
			free(e);
			e = prev;
		}
		prev = e;
	}
}
void update_bullets(void)
{
	Entity* b;
	Entity* prev = &bulletList.head;
	for (b = bulletList.head.next; b != NULL; b = b->next)
	{
		b->x += b->dx * delta_time;
		b->y += b->dy * delta_time;

		/* Check bullet collisions, kills bullet and entity if hit */
		bullet_hit(b);
		
		/* cleanup bullets */
		if (b->x > WINDOW_W || b->x < (0 - b->w) || b->health <= 0)
		{
			if (b == bulletList.tail)
			{
				bulletList.tail = prev;
			}
			prev->next = b->next;
			free(b);
			b = prev;
		}
		prev = b;
	}
}

void update_difficulty(void)
{
	if ( --difficulty_timer <= 0 )
	{
		if (enemy_wave) difficulty_timer = enemy_wave_difficulty_delay + 10 * waves_survived; /* make waves last longer with more survived */
		else difficulty_timer = difficulty_delay;
		
		if (enemy_wave) difficulty -= 0.01;
		else difficulty -= 0.05f;

		/* We are in an enemy wave! */
		if ( difficulty <= 0.1f ) { enemy_wave = 1; wave = 1; }
		else enemy_wave = 0;
		
		/* reset difficulty for wave relief */
		if ( difficulty <= 0.00f ) { difficulty = 0.6f; waves_survived += 1; wave_cooldown = 300; }
	}

	/* wave cooldown allows player to cleanup enemies after a wave */
	if ( --wave_cooldown <= 0 ) wave = 0;
}

void update_start(void)
{
	if (game.enter) game.state = PLAY;
}
void update_game_over(void)
{
	if (game.enter)
	{
		setup_game();
		game.state = PLAY;
	}
}
void update(void)
{
	UpdateMusicStream(soundtrack);
	delta_time = GetFrameTime();
	switch (game.state)
	{
	case START:
		update_start();
		break;
	case PLAY:
		spawn_enemy();
		update_player();
		update_bullets();
		update_enemies();
		update_difficulty();
		break;
	case GAME_OVER:
		update_game_over();
		break;
	}
}

/*********************************************************************************************
*	Render Functions
**********************************************************************************************/
void render_entities(void)
{
	Entity* e = player;
	
	/* Player */
	DrawRectangle(e->x, e->y, e->w, e->h, e->color);

	/* Enemies */
	for (e = enemyList.head.next; e != NULL; e = e->next)
	{
		DrawRectangle(e->x, e->y, e->w, e->h, e->color);
	}

	/* Bullets */
	for (e = bulletList.head.next; e != NULL; e = e->next)
	{
		DrawRectangle(e->x, e->y, e->w, e->h, e->color);
	}
}
void render_ui(void)
{
	DrawText(TextFormat("Score: %d", score), 16, 16, 24, GREEN);
	if ( enemy_wave ) DrawText("WAVE!!!!!", 256, 16, 36, RED);
}
void render_start(void)
{
	int i = 0;
	DrawText("2D Side Scroller - GameDevTV GameJam 2023", 32, 16 + 64*i, 48, YELLOW);
	i++;
	DrawText("By DMBALZER", 64, 16 + 64*i, 36, YELLOW);
	i++;
	DrawText("Game Engine: Raylib - https://www.raylib.com/", 64, 16 + 64*i, 24, GREEN);
	i++;
	DrawText("Soundtrack - Space Fun By Crowander", 64, 16 + 64*i, 24, GREEN);
	i++;
	DrawText("- https://freemusicarchive.org/music/crowander/uplifting-funband-poprock/space-fun/", 16+8, 16 + 64*i, 16, GREEN);
	i++;
	DrawText("Laser Sound Effect - 200 Free SFX  - https://kronbits.itch.io/freesfx", 64, 16 + 64*i, 24, GREEN);
	i++;
	DrawText("Press Enter to Play!", 64, 16 + 64*i, 48, GREEN);
	i++;
	DrawText("Arrow Keys to move.  Z to Shoot", 64, 16 + 64*i, 48, RAYWHITE);
	i++;
	DrawText("Click in Window to hear sounds", 64, 16 + 64*i, 48, RAYWHITE);
	i++;
}
void render_game_over(void)
{
	int i = 0;
	DrawText("Game Over!", 128, 64 + 64*i, 48 ,RED);
	i++;
	DrawText(TextFormat("Final Score: %d", score), 128, 64 + 64*i, 48 ,GREEN);
	i++;
	DrawText(TextFormat("Waves Survived: %d", waves_survived), 128, 64 + 64*i, 48 ,BLUE);
	i++;
	DrawText("Press Enter to Play Again!", 128, 64 + 64*i, 48, RAYWHITE);
	i++;
	if (retries >= 2)
		DrawText(TextFormat("Having Trouble?\nMaybe things will be a bit easier by\npressing a certain (GameJam THEME) key!"), 32, 64 + 64*i, 24, YELLOW);
}
void render(void)
{
	BeginDrawing();
	ClearBackground(BLACK);

	switch (game.state)
	{
	case START:
		render_start();
		break;
	case PLAY:
		render_entities();
		render_ui();
		break;
	case GAME_OVER:
		render_game_over();
		break;
	}

	EndDrawing();
}

/*********************************************************************************************
*	Cleanup Functions
**********************************************************************************************/
void clean_lists(void)
{
	Entity* e = enemyList.head.next;

	while (e != NULL)
	{
		Entity* tmp = e;
		e = e->next;
		free(tmp);
	}
	enemyList.head.next = NULL;
	enemyList.tail = &enemyList.head;

	e = bulletList.head.next;
	while (e != NULL)
	{
		Entity* tmp = e;
		e = e->next;
		free(tmp);
	}
	bulletList.head.next = NULL;
	bulletList.tail = &bulletList.head;
}
void cleanup(void)
{
	/* Cleanliness is next to godliness */
	UnloadMusicStream(soundtrack);
	UnloadSound(laserSound);
	free(player);
	clean_lists();
	CloseWindow();
}

/*********************************************************************************************
*	Main Game Function
**********************************************************************************************/
int main(int argc, char** argv)
{
	initialize_game();
	running = setup_game();

	while (running && !WindowShouldClose())
	{
		process_input();
		update();
		render();
	}

	cleanup();

	return 0;
}