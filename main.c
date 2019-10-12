#include <pebble.h>

// -------------------------------------------------------------------// Globals
//
  
Window *my_window;
Layer* g_pDrawingLayer;
GPath* g_pIsometricBlock;
GPath* g_pExitMarker;

GBitmap* g_pBitmapPlayerNE_Sprite;
GBitmap* g_pBitmapPlayerNW_Sprite;
GBitmap* g_pBitmapPlayerSW_Sprite;
GBitmap* g_pBitmapPlayerSE_Sprite;
GBitmap* g_pBitmapPlayerNE_Mask;
GBitmap* g_pBitmapPlayerNW_Mask;
GBitmap* g_pBitmapPlayerSW_Mask;
GBitmap* g_pBitmapPlayerSE_Mask;

GBitmap* g_pBitmapEnemyUnit1_Sprite;
GBitmap* g_pBitmapEnemyUnit1_Mask;
GBitmap* g_pBitmapEnemyUnit2_Sprite;
GBitmap* g_pBitmapEnemyUnit2_Mask;
GBitmap* g_pBitmapEnemyUnit3_Sprite;
GBitmap* g_pBitmapEnemyUnit3_Mask;
GBitmap* g_pBitmapEnemyUnit4_Sprite;
GBitmap* g_pBitmapEnemyUnit4_Mask;

GBitmap* g_pBitmapTreasureGem_Mask;
GBitmap* g_pBitmapTreasureGem_Sprite;

static const int c_nTileWidth = 16;
static const int c_nTileHeight = 10;
static const int c_nSpriteDimensionPx = 16;

static const uint32_t c_nHighScoreKey = 1009966;

static const GPathInfo ISOBLOCK = {
  .num_points = 6,
  .points = (GPoint []) { { 0, 5 }, 
                          { 8, 0}, 
                          { 16, 5 }, 
                          { 16, 9}, 
                          { 8, 14 }, 
                          { 0, 9} }
};

static const GPathInfo EXITMARKER = {
  .num_points = 7,
  .points = (GPoint []) { { 1, 2 }, 
                          { 3, 2 }, 
                          { 3, 0 }, 
                          { 6, 3 }, 
                          { 3, 6 }, 
                          { 3, 4 }, 
                          { 0, 4 } }
};

static const uint32_t const g_vibePatternBlockRemoved[] = { 200, 100, 50, 50 };

VibePattern g_vibPatternBlockRemovedStruct = {
  .durations = g_vibePatternBlockRemoved,
  .num_segments = ARRAY_LENGTH(g_vibePatternBlockRemoved),
};

#define cnArrayWidth 8
#define cnArrayHeight 8
  
typedef enum  
{
  eEntityNone = 0,
  eEntityPlayer = 1,
  eEntityEnemy = 2,
  eTreasureGem = 3
} EEntityType;
  
static bool g_nMap[ cnArrayWidth ][ cnArrayHeight ];
static EEntityType g_nNonPlayerEntities[ cnArrayWidth ][ cnArrayHeight ];

static const int c_nScoreStep = 10;
static const int c_nScoreTreasure = 100;
static const int c_nScoreLandCreationPenalty = 100;

static bool g_fBounceSpritesThisSecond = false;

typedef enum 
{
  eEntityFacingNE = 0,
  eEntityFacingNW = 1,
  eEntityFacingSE = 2,
  eEntityFacingSW = 3
} EEntityDirectionFacing;

struct EntityPos
{
  int m_nX;  
  int m_nY;
  
  EEntityDirectionFacing m_eDirectionFacing;
};

struct GameOptions
{
  int m_nScore;
  struct EntityPos m_playerObj;
  struct EntityPos m_exitObj;
  
  struct EntityPos m_enemiesArray[ 8 ];
  int m_nNumberOfEnemies;
  
  bool m_fCanLevelBeExited;
  int m_nHighScore;
  bool m_fGameOver;
};

static struct GameOptions g_gameOptions;

// -------------------------------------------------------------------
// Forward decls
//

void DrawIsoTiles( GContext* ctx );
void display_layer_update_callback(Layer* pLayer, GContext* ctx);
void GenerateNewMap();
static void tick_handler(struct tm *tick_time, TimeUnits units_changed);
void config_provider(Window* pWindow) ;
void select_single_click_handler( ClickRecognizerRef recognizer, void *context );
void down_single_click_handler( ClickRecognizerRef recognizer, void *context );
void middle_single_click_handler( ClickRecognizerRef recognizer, void *context );
void up_single_click_handler( ClickRecognizerRef recognizer, void *context );
void UpdatePlayerDirectionFacing( bool fUp );
void DrawTileEntity( EEntityType eType, 
                     int nXPx, 
                     int nYPx, 
                     EEntityDirectionFacing eDirectionFacing, 
                     GContext* ctx );
void HandlePlayerMove();
void DrawHUD( GContext* ctx );
bool CheckIfLevelIsComplete();
bool IsTreasure( EEntityType eEntityType );
void HandleEnemyUnitMove( int* nXPos, 
                          int* nYPos, 
                         EEntityDirectionFacing* peDirectionFacing );
bool HandleEntityMove( int nEntityXCoord, 
                       int nEntityYCoord,
                       EEntityDirectionFacing eDirection,
                       bool fIsPlayer,
                       int* pnNewEntityXPos,
                       int* pnNewEntityYPos,
                       bool fCreateNewLandIfInvalidMove );
void TickEnemyUnits();
void DrawGameOverScreen( GContext* ctx );
void ResetGame();

// -------------------------------------------------------------------
// Functions
//

void handle_init(void) 
{
  //
  // Create window
  //
  
  my_window = window_create();
  window_set_background_color(my_window, GColorBlack);
  window_stack_push(my_window, true);
  
  //
  // Create layer for drawing
  //
  
  Layer *root_layer = window_get_root_layer(my_window);
  GRect frame = layer_get_frame(root_layer);

  g_pDrawingLayer = layer_create(frame);
  layer_set_update_proc(g_pDrawingLayer, &display_layer_update_callback);
  layer_add_child(root_layer, g_pDrawingLayer);
  
  //
  //  Create polygon definition
  //
  
  g_pIsometricBlock = gpath_create( &ISOBLOCK );
  g_pExitMarker = gpath_create( &EXITMARKER );
  
  //
  //  Initialise contents of map
  //
  
  srand( time( NULL ) );
  GenerateNewMap();
  
  //
  //  Register with the tick handler
  //
  
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
  
  //
  //  Register graphics resources
  //
  
  g_pBitmapPlayerNE_Sprite = gbitmap_create_with_resource( RESOURCE_ID_img_penguin_ne_sprite );
  g_pBitmapPlayerNE_Mask = gbitmap_create_with_resource( RESOURCE_ID_img_penguin_ne_mask );
  
  g_pBitmapPlayerNW_Sprite = gbitmap_create_with_resource( RESOURCE_ID_img_penguin_nw_sprite );
  g_pBitmapPlayerNW_Mask = gbitmap_create_with_resource( RESOURCE_ID_img_penguin_nw_mask );
   
  g_pBitmapPlayerSW_Sprite = gbitmap_create_with_resource( RESOURCE_ID_img_penguin_sw_sprite );
  g_pBitmapPlayerSW_Mask = gbitmap_create_with_resource( RESOURCE_ID_img_penguin_sw_mask );
 
  g_pBitmapPlayerSE_Sprite = gbitmap_create_with_resource( RESOURCE_ID_img_penguin_se_sprite );
  g_pBitmapPlayerSE_Mask = gbitmap_create_with_resource( RESOURCE_ID_img_penguin_se_mask );
  
  g_pBitmapEnemyUnit1_Sprite = gbitmap_create_with_resource( RESOURCE_ID_enemy_skeleton_se_sprite_1 );
  g_pBitmapEnemyUnit1_Mask = gbitmap_create_with_resource( RESOURCE_ID_enemy_skeleton_se_mask_1 );
  g_pBitmapEnemyUnit2_Sprite = gbitmap_create_with_resource( RESOURCE_ID_enemy_skeleton_se_sprite_2 );
  g_pBitmapEnemyUnit2_Mask = gbitmap_create_with_resource( RESOURCE_ID_enemy_skeleton_se_mask_2 );
  
  g_pBitmapEnemyUnit3_Sprite = gbitmap_create_with_resource( RESOURCE_ID_enemy_skeleton_sw_sprite_1 );
  g_pBitmapEnemyUnit3_Mask = gbitmap_create_with_resource( RESOURCE_ID_enemy_skeleton_sw_mask_1 );
  g_pBitmapEnemyUnit4_Sprite = gbitmap_create_with_resource( RESOURCE_ID_enemy_skeleton_sw_sprite_2 );
  g_pBitmapEnemyUnit4_Mask = gbitmap_create_with_resource( RESOURCE_ID_enemy_skeleton_sw_mask_2 );
    
  g_pBitmapTreasureGem_Mask = gbitmap_create_with_resource( RESOURCE_ID_img_treasuregem_mask );
  g_pBitmapTreasureGem_Sprite = gbitmap_create_with_resource( RESOURCE_ID_img_treasuregem_sprite );
      
  //
  // Register with the click handler
  //
  
  window_set_click_config_provider( my_window, (ClickConfigProvider) config_provider);
  
  //
  // Grab high score
  //
  
  if( persist_exists( c_nHighScoreKey ) )
  {
    g_gameOptions.m_nHighScore = persist_read_int( c_nHighScoreKey );    
  }
  else
  {
    g_gameOptions.m_nHighScore = 0;
  }
  
  ResetGame();
}

void ResetGame()
{
  g_gameOptions.m_fGameOver = false;
  
  if( g_gameOptions.m_nScore > g_gameOptions.m_nHighScore )
  {
    persist_write_int( c_nHighScoreKey, g_gameOptions.m_nScore ); 
    g_gameOptions.m_nHighScore = g_gameOptions.m_nScore;
  }
  
  g_gameOptions.m_nScore = 0;
  
  GenerateNewMap();
}

void config_provider(Window* pWindow) 
{
  // single click registrations for callback
  
  window_single_click_subscribe(BUTTON_ID_DOWN, down_single_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, middle_single_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_single_click_handler);
}

void down_single_click_handler( ClickRecognizerRef recognizer, void *context ) 
{
  Window *window = (Window *)context;
  
  if( ! g_gameOptions.m_fGameOver )
  {
    UpdatePlayerDirectionFacing( false );
  }
  else
  {
    ResetGame();
  }
}

void middle_single_click_handler( ClickRecognizerRef recognizer, void *context ) 
{
  Window *window = (Window *)context;
  
  if( ! g_gameOptions.m_fGameOver )
  {
    HandlePlayerMove();
  }
  else
  {
    ResetGame();
  }
}

void up_single_click_handler( ClickRecognizerRef recognizer, void *context ) 
{
  Window *window = (Window *)context;
  
  if( ! g_gameOptions.m_fGameOver )
  {
    UpdatePlayerDirectionFacing( true );
  }
  else
  {
    ResetGame();
  }
}

bool HandleEntityMove( int nEntityXCoord, 
                       int nEntityYCoord,
                       EEntityDirectionFacing eDirection,
                       bool fIsPlayer,
                       int* pnNewEntityXPos,
                       int* pnNewEntityYPos,
                       bool fCreateNewLandIfInvalidMove )
{
  switch( eDirection )
  {
    default:
    case eEntityFacingNE:
      ++nEntityYCoord;
      break;
    
    case eEntityFacingNW:
      --nEntityXCoord;
      break;
    
    case eEntityFacingSW:
      --nEntityYCoord;
      break;
    
    case eEntityFacingSE:
      ++nEntityXCoord;
      break;
  }
  
  bool fMoveLegal = g_nMap[ nEntityXCoord ][ nEntityYCoord ];
  
  if(    nEntityXCoord < 0
      || nEntityXCoord == cnArrayWidth
      || nEntityYCoord < 0
      || nEntityYCoord == cnArrayHeight  )
  {
    // Out of bounds
    return false;
  }
  
  if(    ! fMoveLegal 
      && nEntityXCoord >= 0
      && nEntityXCoord < cnArrayWidth
      && nEntityYCoord >= 0
      && nEntityYCoord < cnArrayHeight
      && fCreateNewLandIfInvalidMove )
  {
    // Create new land
    g_nMap[ nEntityXCoord ][ nEntityYCoord ] = true;
    fMoveLegal = true;
    
    g_gameOptions.m_nScore -= c_nScoreLandCreationPenalty;
  }
  
  if( fMoveLegal )
  {
    // Move legal because position exisits
    
    EEntityType eEntityAtDestination = g_nNonPlayerEntities[ nEntityXCoord ][ nEntityYCoord ];
    
    if( eEntityAtDestination == eEntityEnemy )
    {
      // Stop enemies merging and players walking in to enemies
      return false;
    }
    
    // Move valid
    
    if(    pnNewEntityXPos
        && pnNewEntityYPos )
    {
      *pnNewEntityXPos = nEntityXCoord;
      *pnNewEntityYPos = nEntityYCoord;
    }
    
    return true;
  }
  
  // Move invalid
  return false;
}

void TickEnemyUnits()
{
  for( int nEnemy = 0 ; nEnemy < g_gameOptions.m_nNumberOfEnemies ; ++nEnemy )
  {
    bool fMove = ( rand() % 2 == 0 );
    
    if( fMove )
    {
      // Move the unit
      
      HandleEnemyUnitMove( &g_gameOptions.m_enemiesArray[ nEnemy ].m_nX,
                           &g_gameOptions.m_enemiesArray[ nEnemy ].m_nY,
                           &g_gameOptions.m_enemiesArray[ nEnemy ].m_eDirectionFacing );
    }
  }
}

void GetDisanceAndDirectionToPlayer( int nXPx, int nYPx, EEntityDirectionFacing* peDirection, int* pnDistance )
{
  *pnDistance = ( abs( nXPx - g_gameOptions.m_playerObj.m_nX ) 
                  > abs( nYPx - g_gameOptions.m_playerObj.m_nY ) ) ? abs( nXPx - g_gameOptions.m_playerObj.m_nX )
                                                                   : abs( nYPx - g_gameOptions.m_playerObj.m_nY );
  
  if( nXPx - g_gameOptions.m_playerObj.m_nX > 0 )
  {
      // West
      if( nYPx - g_gameOptions.m_playerObj.m_nY > 0 )
      {
          *peDirection = eEntityFacingSW;
      }
      else
      {
          *peDirection = eEntityFacingNW;
      }
  }
  else
  {
      // East
      if( nYPx - g_gameOptions.m_playerObj.m_nY > 0 )
      {
          *peDirection = eEntityFacingSE;
      }
      else
      {
          *peDirection = eEntityFacingNE;
      }
  }
}

void HandleEnemyUnitMove( int* pnXPos, int* pnYPos, EEntityDirectionFacing* peDirectionFacing )
{  
  int nOldPosX = *pnXPos;
  int nOldPosY = *pnYPos;
  
  bool fChangeDirection = rand() % 3 == 0;
  
  if( fChangeDirection )
  {
    *peDirectionFacing = (EEntityDirectionFacing)( rand() & (int)eEntityFacingSW );
  }
  
  // See if the player is nearby
  EEntityDirectionFacing eDirectionToPlayer = eEntityFacingNW;
  int nDistanceToPlayer = 10;
  
  GetDisanceAndDirectionToPlayer( nOldPosX, nOldPosY, &eDirectionToPlayer, &nDistanceToPlayer );
  
  if( nDistanceToPlayer <= 5 )
  {
    // Direct entity towards player
    
    *peDirectionFacing = eDirectionToPlayer;
  }
    
  bool fResult = HandleEntityMove( *pnXPos,
                                   *pnYPos,
                                   *peDirectionFacing,
                                   false, // Is NOT player
                                   pnXPos,
                                   pnYPos,
                                   false );  
  
  if( ! fResult )
  {
    // Not a valid move
    return;
  }
  
  if( rand() % 10 == 0 )
  {
    // Every 10 steps destroy a tile
    g_nMap[ nOldPosX ][ nOldPosY ] = false;
    
    vibes_enqueue_custom_pattern( g_vibPatternBlockRemovedStruct );
  }
  
  // Update enemy position
  
  g_nNonPlayerEntities[ nOldPosX ][ nOldPosY ] = eEntityNone;
  g_nNonPlayerEntities[ *pnXPos ][ *pnYPos ] = eEntityEnemy;
  
  // Check if we have caught the player
  
  if(    g_gameOptions.m_playerObj.m_nX == *pnXPos
      && g_gameOptions.m_playerObj.m_nY == *pnYPos )
  {
    // The player has been killed
      
    g_gameOptions.m_fGameOver = true;
    vibes_long_pulse();
  }
}

void HandlePlayerMove()
{
  // Create new land at cost to score
  bool fCanCreateNewLand = ( g_gameOptions.m_nScore >= c_nScoreLandCreationPenalty );
  
  bool fResult = HandleEntityMove( g_gameOptions.m_playerObj.m_nX,
                                   g_gameOptions.m_playerObj.m_nY,
                                   g_gameOptions.m_playerObj.m_eDirectionFacing,
                                   true, // Is player
                                   &g_gameOptions.m_playerObj.m_nX,
                                   &g_gameOptions.m_playerObj.m_nY,
                                   fCanCreateNewLand );
  
  if( fResult )
  {
    // Perform post move player specific logic
    // Check to see if there is a gem here
    
    // Add step score!  
    g_gameOptions.m_nScore += c_nScoreStep;
    
    // Check and retreieve treasure if required
    if( IsTreasure( g_nNonPlayerEntities[ g_gameOptions.m_playerObj.m_nX ][ g_gameOptions.m_playerObj.m_nY ] ) )
    {
      // Collect treasure!  
      g_gameOptions.m_nScore += c_nScoreTreasure;
      
      g_nNonPlayerEntities[ g_gameOptions.m_playerObj.m_nX ][ g_gameOptions.m_playerObj.m_nY ] = eEntityNone;
      
      // Check to see if there are any more gems in the world, if not, mark level
      // as being exit-able 
      if( CheckIfLevelIsComplete() )
      {
        g_gameOptions.m_fCanLevelBeExited = true;
      }
    }
    
    // Check and exit level if possible
    if(    g_gameOptions.m_fCanLevelBeExited
        && g_gameOptions.m_playerObj.m_nX == g_gameOptions.m_exitObj.m_nX 
        && g_gameOptions.m_playerObj.m_nY == g_gameOptions.m_exitObj.m_nY )
    {
      // We have finished this level, jolly good show  
      GenerateNewMap();
    }
    
    layer_mark_dirty( g_pDrawingLayer );
  }
  else
  {
    vibes_short_pulse();
  }
}

void GenerateNewMap()
{ 
  for( int x = 0 ; x < cnArrayWidth ; ++x )  
  {
    for( int y = 0 ; y < cnArrayHeight ; ++y )  
    {
        bool fValidPosition = rand() % 6 != 0;
        g_nMap[ x ][ y ] = fValidPosition;
      
        g_nNonPlayerEntities[ x ][ y ] = eEntityNone;
      
        if(    fValidPosition        // don't spawn treasure on invalid positions
            && rand() % 5 == 0 ) 
        {
          g_nNonPlayerEntities[ x ][ y ] = eTreasureGem;
        }
    }   
  }
  
  // TODO check player start is in valid place
  g_gameOptions.m_playerObj.m_nX = rand() % (int)cnArrayWidth;
  g_gameOptions.m_playerObj.m_nY = rand() % (int)cnArrayHeight;
  
  g_gameOptions.m_playerObj.m_eDirectionFacing = eEntityFacingSE;
  
  // TODO check exit is in valid place
  g_gameOptions.m_exitObj.m_nX = rand() % (int)cnArrayWidth;
  g_gameOptions.m_exitObj.m_nY = rand() % (int)cnArrayHeight;
  
  g_gameOptions.m_fCanLevelBeExited = false;
  
  // Generate a few enemies
  g_gameOptions.m_nNumberOfEnemies = 0;
  
  int nNumEnemiesToGenerate = 2 + rand() % 2;
  
  for( int nEnemy = 0 ; nEnemy < nNumEnemiesToGenerate ; ++nEnemy )
  {
    int nEnemyXPos = rand() % cnArrayWidth;
    int nEnemyYPos = rand() % cnArrayHeight;
    
    g_gameOptions.m_enemiesArray[ nEnemy ].m_nX = nEnemyXPos;
    g_gameOptions.m_enemiesArray[ nEnemy ].m_nY = nEnemyYPos;
    
    ++g_gameOptions.m_nNumberOfEnemies;
  }
}

void display_layer_update_callback(Layer* pLayer, GContext* ctx) 
{
  if( pLayer == g_pDrawingLayer )
  {
    if( g_gameOptions.m_fGameOver )
    {
      DrawGameOverScreen( ctx );
    }
    else
    {
      // Repaint the isometric view
      DrawIsoTiles( ctx );
      
      // Draw the score and HUD
      DrawHUD( ctx );
    }
  }
}

void DrawGameOverScreen( GContext* ctx )
{
  static char szGameOverText[] =   "Game Over!";
  static char szScoreBufferScore[] =   "Final Score : 00000000";
  static char szHighScoreText[] =   "** New high score!! **";
  static char szPlayAgainText[] =   "Press any key to play again...";
  
  snprintf( &szScoreBufferScore[ 0 ],
              sizeof( szScoreBufferScore ),
              "Final Score : %d",
              g_gameOptions.m_nScore );
  
  GRect rectTextPos = GRect(  0,  
                              5,
                              130, 
                              40 );
  
  graphics_context_set_text_color( ctx, GColorWhite );
  
  graphics_draw_text( ctx,
                      &szGameOverText[0],
                      fonts_get_system_font( FONT_KEY_GOTHIC_18 ),
                      rectTextPos,
                      GTextOverflowModeTrailingEllipsis ,
                      GTextAlignmentCenter,
                      NULL );
  
  rectTextPos.origin.y += 25;
  
  graphics_draw_text( ctx,
                      &szScoreBufferScore[0],
                      fonts_get_system_font( FONT_KEY_GOTHIC_18 ),
                      rectTextPos,
                      GTextOverflowModeTrailingEllipsis ,
                      GTextAlignmentCenter,
                      NULL );
  
  rectTextPos.origin.y += 25;
  
  if(    g_gameOptions.m_nScore > g_gameOptions.m_nHighScore
      && ! g_fBounceSpritesThisSecond )
  {
    graphics_draw_text( ctx,
                        &szHighScoreText[0],
                        fonts_get_system_font( FONT_KEY_GOTHIC_18 ),
                        rectTextPos,
                        GTextOverflowModeTrailingEllipsis ,
                        GTextAlignmentCenter,
                        NULL );
  }
  
  rectTextPos.origin.y += 25;
  
  graphics_draw_text( ctx,
                      &szPlayAgainText[0],
                      fonts_get_system_font( FONT_KEY_GOTHIC_18 ),
                      rectTextPos,
                      GTextOverflowModeTrailingEllipsis ,
                      GTextAlignmentCenter,
                      NULL );
}

bool IsTreasure( EEntityType eEntityType )
{
  switch( eEntityType )
  {
    case eTreasureGem:  
      return true;
    
    default:
      return false;
  }
  
  return false;
}

bool CheckIfLevelIsComplete()
{
  bool fTreasureFound = false;
  
  for( int x = 0 ; x < cnArrayWidth ; ++x )  
  {
    for( int y = 0 ; y < cnArrayHeight ; ++y )  
    {
        if( IsTreasure( g_nNonPlayerEntities[ x ][ y ] ) )
        {
          // Level not yet complete
          return false;
        }
    }
  }
  
  return true;
}

void DrawHUD( GContext* ctx )
{
  static char szScoreBufferHiScore[] = "High Score : 00000000";
  static char szScoreBufferScore[] =   "Score : 00000000";
  
  snprintf( &szScoreBufferHiScore[ 0 ],
              sizeof( szScoreBufferHiScore ),
              "High Score : %d",
              g_gameOptions.m_nHighScore );
  
  snprintf( &szScoreBufferScore[ 0 ],
              sizeof( szScoreBufferScore ),
              "Score : %d",
              g_gameOptions.m_nScore );
  
  GRect rectTextPos = GRect(  0,  
                              0,
                              144, 
                              40 );
  
  graphics_context_set_text_color( ctx, GColorWhite );
  
  graphics_draw_text( ctx,
                      &szScoreBufferHiScore[0],
                      fonts_get_system_font( FONT_KEY_GOTHIC_18 ),
                      rectTextPos,
                      GTextOverflowModeTrailingEllipsis ,
                      GTextAlignmentCenter,
                      NULL );
  
  rectTextPos.origin.y += 18;

  graphics_draw_text( ctx,
                      &szScoreBufferScore[0],
                      fonts_get_system_font( FONT_KEY_GOTHIC_18 ),
                      rectTextPos,
                      GTextOverflowModeTrailingEllipsis ,
                      GTextAlignmentCenter,
                      NULL );
  
}

void UpdatePlayerDirectionFacing( bool fUp )
{
  if( fUp )
  {
    // Perform anti clockwise rotation
    // NE .. NW .. SW .. SE ...
    switch( g_gameOptions.m_playerObj.m_eDirectionFacing )
    {
      default:
      case eEntityFacingNE:
        g_gameOptions.m_playerObj.m_eDirectionFacing = eEntityFacingNW;
        break;
      
      case eEntityFacingNW:
        g_gameOptions.m_playerObj.m_eDirectionFacing = eEntityFacingSW;
        break;
      
      case eEntityFacingSW:
        g_gameOptions.m_playerObj.m_eDirectionFacing = eEntityFacingSE;
        break;
      
      case eEntityFacingSE:
        g_gameOptions.m_playerObj.m_eDirectionFacing = eEntityFacingNE;
        break;
    }
  }
  else
  {
    // Perform clockwise rotation
    // NE .. SE .. SW .. NW ...
    
    switch( g_gameOptions.m_playerObj.m_eDirectionFacing )
    {
      default:
      case eEntityFacingNE:
        g_gameOptions.m_playerObj.m_eDirectionFacing = eEntityFacingSE;
        break;
      
      case eEntityFacingNW:
        g_gameOptions.m_playerObj.m_eDirectionFacing = eEntityFacingNE;
        break;
      
      case eEntityFacingSW:
        g_gameOptions.m_playerObj.m_eDirectionFacing = eEntityFacingNW;
        break;
      
      case eEntityFacingSE:
        g_gameOptions.m_playerObj.m_eDirectionFacing = eEntityFacingSW;
        break;
    }
  }
  
  layer_mark_dirty( g_pDrawingLayer );
}

void DrawIsoObject( int nXPx, int nYPx, GContext* ctx )
{
  gpath_move_to( g_pIsometricBlock, GPoint( nXPx, nYPx ) );
  
  // Fill the path:
  graphics_context_set_fill_color(ctx, GColorWhite);
  gpath_draw_filled(ctx, g_pIsometricBlock);
  // Stroke the path:
  graphics_context_set_stroke_color(ctx, GColorBlack);
  gpath_draw_outline(ctx, g_pIsometricBlock);
  
  // draw face outlines
  graphics_draw_line( ctx,
                      GPoint( nXPx + 0,
                              nYPx + 5 ),
                      GPoint( nXPx + 8,
                              nYPx + 10 ) );
  
  graphics_draw_line( ctx,
                      GPoint( nXPx + 8,
                              nYPx + 10 ),
                      GPoint( nXPx + 16,
                              nYPx + 5 ) );
  
  graphics_draw_line( ctx,
                      GPoint( nXPx + 8,
                              nYPx + 10 ),
                      GPoint( nXPx + 8,
                              nYPx + 21 ) );
}

void DrawExitMarker( int nXPx, int nYPx, GContext* ctx )
{
  if( ! g_gameOptions.m_fCanLevelBeExited )
  {
    // don't draw yet
    return;
  }
  
  gpath_move_to( g_pExitMarker, GPoint( nXPx, nYPx ) );
  
  // Fill the path:
  if( g_fBounceSpritesThisSecond )
  {
    graphics_context_set_fill_color(ctx, GColorWhite );
  }
  else
  {
    graphics_context_set_fill_color(ctx, GColorBlack );
  }
  
  gpath_draw_filled(ctx, g_pExitMarker);
  // Stroke the path:
  graphics_context_set_stroke_color(ctx, GColorBlack);
  gpath_draw_outline(ctx, g_pExitMarker);
}

void DrawTileEntity( EEntityType eType, 
                     int nXPx, 
                     int nYPx, 
                     EEntityDirectionFacing eDirectionFacing, 
                     GContext* ctx )
{
  GBitmap* pBitmapToDrawMask = NULL;
  GBitmap* pBitmapToDrawSprite = NULL;
  
  int nYBounceMassage = 0;
  
  switch( eType )
  {
    case eEntityPlayer:
      switch( eDirectionFacing )
      {
        case eEntityFacingNE:
          pBitmapToDrawMask = g_pBitmapPlayerNE_Mask;
          pBitmapToDrawSprite = g_pBitmapPlayerNE_Sprite;
          break;
        
        case eEntityFacingNW:
          pBitmapToDrawMask = g_pBitmapPlayerNW_Mask;
          pBitmapToDrawSprite = g_pBitmapPlayerNW_Sprite;
          break;
        
        case eEntityFacingSE:
          pBitmapToDrawMask = g_pBitmapPlayerSE_Mask;
          pBitmapToDrawSprite = g_pBitmapPlayerSE_Sprite;
          break;
        
        case eEntityFacingSW:
          pBitmapToDrawMask = g_pBitmapPlayerSW_Mask;
          pBitmapToDrawSprite = g_pBitmapPlayerSW_Sprite;
          break;
      }
      break;
    
    case eEntityEnemy:
      switch( eDirectionFacing )
      {
        case eEntityFacingNE:
        case eEntityFacingSE:
          if( g_fBounceSpritesThisSecond )
          {
            pBitmapToDrawMask = g_pBitmapEnemyUnit1_Mask;
            pBitmapToDrawSprite = g_pBitmapEnemyUnit1_Sprite;
          }
          else
          {
            pBitmapToDrawMask = g_pBitmapEnemyUnit2_Mask;
            pBitmapToDrawSprite = g_pBitmapEnemyUnit2_Sprite;
          }
          break;
        
        case eEntityFacingNW:
        case eEntityFacingSW:
          if( g_fBounceSpritesThisSecond )
          {
            pBitmapToDrawMask = g_pBitmapEnemyUnit3_Mask;
            pBitmapToDrawSprite = g_pBitmapEnemyUnit3_Sprite;
          }
          else
          {
            pBitmapToDrawMask = g_pBitmapEnemyUnit4_Mask;
            pBitmapToDrawSprite = g_pBitmapEnemyUnit4_Sprite;
          }
          break;
      }
      
      break;
    
    case eTreasureGem:
      pBitmapToDrawMask = g_pBitmapTreasureGem_Mask;
      pBitmapToDrawSprite = g_pBitmapTreasureGem_Sprite;
    
      if( g_fBounceSpritesThisSecond )
      {
        nYBounceMassage = 2;
      }
    break;
    
    default:
      // Do nothing
      break;
  }
  
  if(    pBitmapToDrawMask
      && pBitmapToDrawSprite )
  {
    graphics_context_set_compositing_mode( ctx, GCompOpOr );
    
    graphics_draw_bitmap_in_rect( ctx, 
                                  (const GBitmap*) pBitmapToDrawMask,
                                  GRect( nXPx,
                                         nYPx + nYBounceMassage,
                                         c_nSpriteDimensionPx,
                                         c_nSpriteDimensionPx ) );
    
    graphics_context_set_compositing_mode( ctx, GCompOpAnd );
    
    graphics_draw_bitmap_in_rect( ctx, 
                                  (const GBitmap*) pBitmapToDrawSprite,
                                  GRect( nXPx,
                                         nYPx + nYBounceMassage,
                                         c_nSpriteDimensionPx,
                                         c_nSpriteDimensionPx ) );
  }
}

void DrawIsoTiles( GContext* ctx )
{
  const int cnXStartPoint = -5;
  const int cnYStartPoint = 168 / 2;
  
  //
  //  First draw the world
  //
  
  for( int x = 0; x < cnArrayWidth; ++x )
  {
    for( int y = cnArrayHeight; y > 0; --y )
    {
      int nXpositionPx = (y * c_nTileWidth / 2) + (x * c_nTileWidth / 2);
      int nYpositionPx = (x * c_nTileHeight / 2) - (y * c_nTileHeight / 2);
      
      if( g_nMap[ x ][ y - 1 ] )
      {
        DrawIsoObject( cnXStartPoint + nXpositionPx, 
                       cnYStartPoint + nYpositionPx, 
                       ctx );
      }
      
      if(    g_gameOptions.m_exitObj.m_nX == x
          && g_gameOptions.m_exitObj.m_nY == y - 1 )
      {
        // Draw exit position  
        DrawExitMarker( cnXStartPoint + nXpositionPx + 5,
                        cnYStartPoint + nYpositionPx + 2,
                        ctx );
      }
    }
  }
  
  //
  //  Next draw entities on the world
  //
  
  for( int x = 0; x < cnArrayWidth; ++x )
  {
    for( int y = cnArrayHeight; y > 0; --y )
    {
        int nXpositionPx = (y * c_nTileWidth / 2) + (x * c_nTileWidth / 2);
        int nYpositionPx = (x * c_nTileHeight / 2) - (y * c_nTileHeight / 2);
      
        // Draw object 'above' the tile cell but centered half way down
        nYpositionPx -= 8;
      
        // Draw any entities on this block (BUT not the player)
        if( g_nNonPlayerEntities[ x ][ y - 1 ] != eEntityNone )
        {
          EEntityDirectionFacing eDirectionFacing = eEntityFacingNE;
          
          // If this is an enemy unit then get the direction the entity is facing
          int nEntityIndex = 0;
          
          for( ; nEntityIndex < g_gameOptions.m_nNumberOfEnemies ; ++nEntityIndex )
          {
            if(    g_gameOptions.m_enemiesArray[ nEntityIndex ].m_nX == x
                && g_gameOptions.m_enemiesArray[ nEntityIndex ].m_nY == y )
            {
              eDirectionFacing = g_gameOptions.m_enemiesArray[ nEntityIndex ].m_eDirectionFacing;
            }
            
          }
          
          DrawTileEntity( g_nNonPlayerEntities[ x ][ y - 1 ],
                          cnXStartPoint + nXpositionPx, 
                          cnYStartPoint + nYpositionPx, 
                          eDirectionFacing,
                          ctx );
        }
      
        // Draw player position if required
        if(    g_gameOptions.m_playerObj.m_nX == x 
            && g_gameOptions.m_playerObj.m_nY == y - 1 )
        { 
            DrawTileEntity( eEntityPlayer,
                            cnXStartPoint + nXpositionPx, 
                            cnYStartPoint + nYpositionPx, 
                            g_gameOptions.m_playerObj.m_eDirectionFacing,
                            ctx );
         }
    }
  }
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) 
{
  g_fBounceSpritesThisSecond = ! g_fBounceSpritesThisSecond;
  
  if( ! g_gameOptions.m_fGameOver )
  {
    TickEnemyUnits();
  }
  
  // mark view as dirty so we can repaint
  
  layer_mark_dirty( g_pDrawingLayer );
}

void handle_deinit(void) 
{
  // Store the high score
  if( g_gameOptions.m_nScore > g_gameOptions.m_nHighScore )
  {
    persist_write_int( c_nHighScoreKey, g_gameOptions.m_nScore ); 
  }
  
  layer_destroy(g_pDrawingLayer);
  window_destroy(my_window);
  tick_timer_service_unsubscribe();
  
  gbitmap_destroy( g_pBitmapPlayerNE_Sprite );
  gbitmap_destroy( g_pBitmapPlayerNW_Sprite );
  gbitmap_destroy( g_pBitmapPlayerSW_Sprite );
  gbitmap_destroy( g_pBitmapPlayerSE_Sprite );
  gbitmap_destroy( g_pBitmapPlayerNE_Mask );
  gbitmap_destroy( g_pBitmapPlayerNW_Mask );
  gbitmap_destroy( g_pBitmapPlayerSW_Mask );
  gbitmap_destroy( g_pBitmapPlayerSE_Mask );
  gbitmap_destroy( g_pBitmapEnemyUnit1_Sprite );
  gbitmap_destroy( g_pBitmapEnemyUnit1_Mask );
  gbitmap_destroy( g_pBitmapEnemyUnit2_Sprite );
  gbitmap_destroy( g_pBitmapEnemyUnit2_Mask );
  gbitmap_destroy( g_pBitmapEnemyUnit3_Sprite );
  gbitmap_destroy( g_pBitmapEnemyUnit3_Mask );
  gbitmap_destroy( g_pBitmapEnemyUnit4_Sprite );
  gbitmap_destroy( g_pBitmapEnemyUnit4_Mask );
}

int main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}
