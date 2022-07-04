#include "mbed.h"
#include "stm32f413h_discovery_ts.h"
#include "stm32f413h_discovery_lcd.h"

/*
Definisanje tagova za MQTT teme. Teme su razvrstane u tri skupine:
igrac1 i igrac2, te pauza
*/

// ZA MQTT
#define TEMASUBUPPLAYER1 "g3/igrac1/up"
#define TEMASUBDOWNPLAYER1 "g3/igrac1/down"
#define TEMASUBLEFTPLAYER1 "g3/igrac1/left"
#define TEMASUBRIGHTPLAYER1 "g3/igrac1/right"
#define TEMASUBSHOOTPLAYER1 "g3/igrac1/shoot"

#define TEMASUBUPPLAYER2 "g3/igrac2/up"
#define TEMASUBDOWNPLAYER2 "g3/igrac2/down"
#define TEMASUBLEFTPLAYER2 "g3/igrac2/left"
#define TEMASUBRIGHTPLAYER2 "g3/igrac2/right"
#define TEMASUBSHOOTPLAYER2 "g3/igrac2/shoot"

#define TEMASUBPAUSE "g3/pause"

#define MQTTCLIENT_QOS2 0

#include "MQTTNetwork.h"
#include "MQTTmbed.h"
#include "MQTTClient.h"
#include <string.h>

// ZA GAME ENGINE
#include <malloc.h>
#include <memory.h>
#include <vector>

int arrivedcount = 0;
int arrivedcountClient2 = 0;

#define MQTT_CLIENT_NAME "Igrac1"
#define MQTT_CLIENT2_NAME "Igrac2"
#define MQTT_CLIENT3_NAME "Pauza"

/*
    Enum klasa orjentisanje na displeju;
*/

enum Direction{ UP, DOWN, LEFT, RIGHT };


/*
   Klasa kojom će biti modelirani projektili
*/
class Missile{

public:
    
    int x, y;    // Atributi za modeliranje pozicije projektila na ekranu
                 // NJihov updejt će biti izvršen neposredno pri refreshu ekrana
                 
    int speedX, speedY;   // Atributi zaduženi za vođenje evidencije o promjeni pozicije
                          // projektila između dvije uzastopne etape refresha ekrana
                          
                          
    Direction direction;  // Smjer projektila
    

	Missile(Direction dir, int  x, int y) {
		
		this->x = x;
		this->y = y;
		speedX=0;
		speedY = 0;
		direction = dir;
		printf("Pucanje\n");
	}
	  
	void draw(){
	    // Iscrtavanje projektila na ekran
	    // Funkcija će se pozivati prilikom svakog refresh-a ekrana, ako postoji aktivni 
	    // projektil na ekranu.
	    
		BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
		
		if(direction == Direction::UP){
			
			BSP_LCD_FillRect(x-1,y-5,2,5);
		}else if(direction == Direction::DOWN){
			
			BSP_LCD_FillRect(x-1,y,2,5);
		}else if(direction == Direction::LEFT){
			
			BSP_LCD_FillRect(x-5,y-1,5,2);
		}else{
			
			BSP_LCD_FillRect(x,y-1,5,2);
		}
	}
	  
	void updatePos() {
	    // Updejt pozicije projektila
	    // Funkcija će se pozivati prije iscrtavanja aktivnog projektila na ekrana.
	    // Dodaje varijable speedX i speedY koje predstavljaju pomak projektila
	    // između dva refresh-a ekrana na poziciju prilikom prethodnog isrctavanja na ekran
	    
		x += speedX;
		y += speedY;
		speedX = 0;
		speedY = 0;
	}
	  
	  
	void moveUp() { speedY -= 1; }
	  
	void moveDown() { speedY += 1; }
	  
	void moveLeft() { speedX -= 1; }
	  
	void moveRight() { speedX += 1; }

};
// DEFAULT TANK COLORS
// 1 - red, 2 - blue, 3 - green
int tankOneColor = 1, tankTwoColor = 2;

void provjeriPogodenTenk(int tackaX, int tackaY, int color);

/*
   Tickeri za izvršavanje kretanja projektila.
   Ticker će biti aktiviran kada se primi signal 1 preko MQTT. 
   Biti će deaktiviran kada se primi signal 0.
*/
Ticker player1T, player2T;


/*
   Klasa koja će modelirati tenkove
*/


class Tank{
	
    int width, height, speedX, speedY, x, y;
    //
    //  Atributi x,y,speedX, speedY imaju istu ulogu kao kod klase Missle.
    //  Width i Height predstavljaju širinu i visinu tenka.
    //
    int color = 0;   // Boja tenka
    
    Direction direction;     // Smjer prema kojem je tenk okrenut. Tenk će uvijek biti okrenut
                             // prema posljednjem smjeru kretanja
    //Missile* activeMissile = nullptr;
    
    /*
       MissileTicker atribut služi za praćenje i vršenje kretanje projektila.
       Missles vektor služi za manipulisanje sa ispaljenim projektilima.
    */
    Ticker missileTicker;
    vector<Missile> missiles;  
  
public:
    int getX (){
        return x;
    }
    
    
    // Funkcija za reset stanja tenkova, nakon završene igre.
    void reset(){
        missileTicker.detach();
        missiles.clear();
        
        speedX = 0;
        speedY = 0;
        
        if(color == tankOneColor){
            x = 40;
            y = 150;
            direction = Direction::UP;
        }else{
            x = 150;
            y = 40;
            direction = Direction::DOWN;
        }
    }
    
    int getY (){
        return y;
    }
    
	Tank(int width, int height, Direction dir, int  x = 0, int y = 0) {
		
		this->width = width;
		this->height = height;
		this->speedX = 0;
		this->speedY = 0;
		this->x = x;
		this->y = y;
		direction = dir;
	}
	
	void setColor(int color) { this->color = color; }
  
  
  
    // Funkcija za crtanje tenka na ekran
	void draw(){
	   
	    
	    if (color == 1) BSP_LCD_SetTextColor(LCD_COLOR_RED);
		else if (color == 2) BSP_LCD_SetTextColor(LCD_COLOR_BLUE);
		else if (color == 3) BSP_LCD_SetTextColor(LCD_COLOR_GREEN);
		
		BSP_LCD_FillRect(x,y,width,height);
		if(direction == Direction::UP){
		   BSP_LCD_DrawVLine(x+width/2, y-10, 10);
		}		
		else if(direction == Direction::DOWN){
		   BSP_LCD_DrawVLine(x+width/2, y+height, 10);
		} 
		else if(direction == Direction::LEFT){
		   BSP_LCD_DrawHLine(x-10, y+height/2, 10);  
		}	
		else{
		   BSP_LCD_DrawHLine(x+width, y+height/2, 10);  
		}									
		
		 BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
		BSP_LCD_DrawRect(x+5,y+5,10,10);
	}
  
  
    /*
      Funkcija za updejt pozicije tenka na ekranu, te provjera da li se desila kolizija
      Kolizija će se desiti, ako se tenk pronađe na pozicijama ekrana gdje su iscrtne prepreke
    */
	void updatePos() {
	  
		x+= speedX;
		y+= speedY;
		speedX = 0;
		speedY = 0;   
		
		if(provjeraKolizije()){
		    if(color == tankOneColor){
		        player1T.detach();        // Ako se desila kolizija sa preprekom, vršimo zaustavljanje kretanja
		    }else{
		        player2T.detach();
		    }
		}
	}
	
	/* Funkcija za provjeru kolizije*/
	bool provjeraKolizije(){
	    int x1 = x, x2 = x+width;
	    int y1 = y, y2 = y+height;
	    
	    // Provjera gornje lijeve tacke tenka
	    
	    // TEŠKA IFOLOGIJA...
	    
	    	if((x1 > 0 && x1<40 && y1>120 && y1<135) ||  //LEFT BARRIER CHECK
			(x1 > 200 && x1<240 && y1>120 && y1<135) ||  //RIGHT BARRIER CHECK
			(x1 > 113 && x1<128 && y1>0 && y1<40) ||     //TOP BARRIER CHECK
			(x1 > 113 && x1<128 && y1>200 && y1<240) ||  //BOTTOM BARRIER CHECK
			(x1 > 113 && x1<128 && y1>80 && y1<170) ||   //MIDDLE BARRIER CHECK
			(x1 > 75 && x1<165 && y1>120 && y1<135) ||   //MIDDLE BARRIER CHECK
			(x1 > 167 && x1<197 && y1>167 && y1<197)   //BOTTOM RIGHT BARRIER CHECK
			){
				return true;
			}
			
		// Provjera gornje desne tacke tenka
			
			if((x2 > 0 && x2<40 && y1>120 && y1<135) ||  //LEFT BARRIER CHECK
			(x2 > 200 && x2<240 && y1>120 && y1<135) ||  //RIGHT BARRIER CHECK
			(x2 > 113 && x2<128 && y1>0 && y1<40) ||     //TOP BARRIER CHECK
			(x2 > 113 && x2<128 && y1>200 && y1<240) ||  //BOTTOM BARRIER CHECK
			(x2 > 113 && x2<128 && y1>80 && y1<170) ||   //MIDDLE BARRIER CHECK
			(x2 > 75 && x2<165 && y1>120 && y1<135) ||   //MIDDLE BARRIER CHECK
			(x2 > 167 && x2<197 && y1>167 && y1<197)   //BOTTOM RIGHT BARRIER CHECK
			){
				return true;
			}
			
		//Provjera donje lijeve tacke tenka
		
			if((x1 > 0 && x1<40 && y2>120 && y2<135) ||  //LEFT BARRIER CHECK
			(x1 > 200 && x1<240 && y2>120 && y2<135) ||  //RIGHT BARRIER CHECK
			(x1 > 113 && x1<128 && y2>0 && y2<40) ||     //TOP BARRIER CHECK
			(x1 > 113 && x1<128 && y2>200 && y2<240) ||  //BOTTOM BARRIER CHECK
			(x1 > 113 && x1<128 && y2>80 && y2<170) ||   //MIDDLE BARRIER CHECK
			(x1 > 75 && x1<165 && y2>120 && y2<135) ||   //MIDDLE BARRIER CHECK
			(x1 > 167 && x1<197 && y2>167 && y2<197)   //BOTTOM RIGHT BARRIER CHECK
			){
				return true;
			}
			
		//Provjera donje dense tacke tenka
		
			if((x2 > 0 && x2<40 && y2>120 && y2<135) ||  //LEFT BARRIER CHECK
			(x2 > 200 && x2<240 && y2>120 && y2<135) ||  //RIGHT BARRIER CHECK
			(x2 > 113 && x2<128 && y2>0 && y2<40) ||     //TOP BARRIER CHECK
			(x2 > 113 && x2<128 && y2>200 && y2<240) ||  //BOTTOM BARRIER CHECK
			(x2 > 113 && x2<128 && y2>80 && y2<170) ||   //MIDDLE BARRIER CHECK
			(x2 > 75 && x2<165 && y2>120 && y2<135) ||   //MIDDLE BARRIER CHECK
			(x2 > 167 && x2<197 && y2>167 && y2<197)   //BOTTOM RIGHT BARRIER CHECK
			){
				return true;
			}
			
			return false;
	}
   
   
   /*
    
    Glavna funkcija koja će prvo pozivate updejt kordinata, zatim iscrtavati tenk,
    te upravljati sa njegovim projektilom. Bili smo prisiljeni da ogranicimo projektile
    na 1 po tenku.
    
   */
	void updateScreen() {
		this->updatePos();
		this->draw();
		//printf("Slikam tenk\n");
				  
		if(!missiles.empty()){
			
			//printf("Update stanja rakete\n");
			missiles[0].updatePos();
		int x1= missiles[0].x;
		int y1 = missiles[0].y;
		
		// Provjera da li je projektil tenka na poziciji drugog igraća
		provjeriPogodenTenk(x1, y1, color);
		
			// Provjera da li je projektil tenka na poziciji prepreke ili izašao sa mape
		bool obrisano  = false;
		if((x1 > 0 && x1<40 && y1>120 && y1<135) ||  //LEFT BARRIER CHECK
			(x1 > 200 && x1<240 && y1>120 && y1<135) ||  //RIGHT BARRIER CHECK
			(x1 > 113 && x1<128 && y1>0 && y1<40) ||     //TOP BARRIER CHECK
			(x1 > 113 && x1<128 && y1>200 && y1<240) ||  //BOTTOM BARRIER CHECK
			(x1 > 113 && x1<128 && y1>80 && y1<170) ||   //MIDDLE BARRIER CHECK
			(x1 > 75 && x1<165 && y1>120 && y1<135) ||   //MIDDLE BARRIER CHECK
			(x1 > 167 && x1<197 && y1>167 && y1<197) ||   //BOTTOM RIGHT BARRIER CHECK
			x1 > 250 || y1 > 240 || 
		    x1 < -10 || y1 < -10){
				// Ako jeste brišemo projektil
				printf("Brisem\n");
				missileTicker.detach();
				missiles.clear();
				obrisano = true;
			}
			
			if(!obrisano){
			   	missiles[0].draw();
			}
		}
	};
	
  
	void moveUp() {
		  
		speedY--;
		direction = Direction::UP;
	};
  
	void moveDown() {
	  
		speedY++;
		direction = Direction::DOWN; 
	};
  
	void moveLeft() {
	  
		speedX--;
		direction = Direction::LEFT;
	};
  
	void moveRight() {
	  
		speedX++;
		direction = Direction::RIGHT;
	};
  
  
    /*
       Funkcija odgovorna za pucanje, tj. instantaciju Missile klase.
    */
	void shoot() {
		if(missiles.size() != 0)	return;    // Ako je već jedan projektil ispucan, završiti rad
     
     /*
        Provjera smjera tenka, te prosljeđivanje odgovarajućih parametara.
        Prosljeđene kordinate predstavljaju vrh cijevi tenka
        Postavljanje ticker-a za kontrolu pozicije projektila
     */
		if(direction == Direction::UP){
			
			missiles.push_back(Missile(direction, x+width/2,y-10));
			missileTicker.attach_us(Callback<void()>(&missiles[0], &Missile::moveUp), 1e4);
		}else if(direction == Direction::DOWN){
			
			missiles.push_back(Missile(direction, x+width/2,y+height+10));
			missileTicker.attach_us(Callback<void()>(&missiles[0], &Missile::moveDown), 1e4);
		}else if(direction == Direction::LEFT){
			
			missiles.push_back(Missile(direction, x-10,y+height/2));
			missileTicker.attach_us(Callback<void()>(&missiles[0], &Missile::moveLeft), 1e4);	
		}else{
			
			missiles.push_back(Missile(direction, x+width+10,y+height/2));
			missileTicker.attach_us(Callback<void()>(&missiles[0], &Missile::moveRight), 1e4);
		}
	}
};

// Instance tenkova koje će biti korištene
Tank tank1(20, 20, Direction::DOWN, 40, 150);
Tank tank2(20, 20, Direction::DOWN, 150, 40);

// TOUCH SCREEN
TS_StateTypeDef TS_State = { 0 };
void WinningScreen(int color);


/*
  Funkcija za provjeru pogodtka tenka. Prosljeđuje se boja tenka koji je pucao.
*/
void provjeriPogodenTenk(int tackaX, int tackaY, int color){
    int xTenk, yTenk;
        
    if(color == tankOneColor){
        xTenk = tank2.getX();
        yTenk = tank2.getY();
    }else{
        xTenk = tank1.getX();
        yTenk = tank1.getY();
    }
    
    if(tackaX > xTenk && tackaX <xTenk+20 && tackaY>yTenk && tackaY<yTenk+20) 
	{	  
	     WinningScreen(color);
	}
}

// ACTIVE SCREEN
// 1 - MainMenu, 2 - Game, 3 - ChangeColor, 4 - About, 5 - PauseMenu, 6 - WinningScreen
int activeScreen = 1;

void MainMenu() {
    
    // BACKGROUND COLOR
    BSP_LCD_Clear(LCD_COLOR_WHITE);         
    
    // TEXT DRAWING COLOR
    BSP_LCD_SetTextColor(LCD_COLOR_RED);
    
    // WELCOME TEXT 
    BSP_LCD_SetFont(&Font16);
    BSP_LCD_DisplayStringAt(0, 30, (uint8_t *)"T A N K  W A R S", CENTER_MODE);
    
    // TEXT DRAWING COLOR
    BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
    
    // NEW GAME
    BSP_LCD_DisplayStringAt(0, 80, (uint8_t *)"NEW GAME", CENTER_MODE);
    
    // CHANGE COLOR
    BSP_LCD_DisplayStringAt(0, 100, (uint8_t *)"CHANGE COLOR", CENTER_MODE);

    // ABOUT
    BSP_LCD_DisplayStringAt(0, 120, (uint8_t *)"ABOUT", CENTER_MODE);
    
}

void ChangeColor() {
    
    // BACKGROUND COLOR
    BSP_LCD_Clear(LCD_COLOR_WHITE); 
    
    // TEXT DRAWING COLOR
    BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
    
    // BACK BUTTON
    BSP_LCD_DisplayStringAt(0, 200, (uint8_t *)"BACK", CENTER_MODE);
    
    // TANK 1 COLOR
    BSP_LCD_DisplayStringAt(0, 30, (uint8_t *)"TANK 1 COLOR", CENTER_MODE);
    
    // 3 AVAILABLE COLORS
    BSP_LCD_SetTextColor(LCD_COLOR_RED);
    BSP_LCD_FillRect(75,  60, 15, 15);
    BSP_LCD_SetTextColor(LCD_COLOR_BLUE);
    BSP_LCD_FillRect(115, 60, 15, 15);
    BSP_LCD_SetTextColor(LCD_COLOR_GREEN);
    BSP_LCD_FillRect(155, 60, 15, 15);
    
    // TANK 2 COLOR
    BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
    BSP_LCD_DisplayStringAt(0, 120, (uint8_t *)"TANK 2 COLOR", CENTER_MODE);
    
    // 3 AVAILABLE COLORS
    BSP_LCD_SetTextColor(LCD_COLOR_RED);
    BSP_LCD_FillRect(75,  150, 15, 15);
    BSP_LCD_SetTextColor(LCD_COLOR_BLUE);
    BSP_LCD_FillRect(115, 150, 15, 15);
    BSP_LCD_SetTextColor(LCD_COLOR_GREEN);
    BSP_LCD_FillRect(155, 150, 15, 15);
    
    // COLOR FOR OUTSIDE BORDER
    BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
    
    // ADDING BORDER FOR TANK 1 CHOOSEN COLOR
	if (tankOneColor == 1) // red
		BSP_LCD_DrawRect(74,  59,  17, 17);
	else if (tankOneColor == 2) // blue
		BSP_LCD_DrawRect(114, 59,  17, 17);
    else if (tankOneColor == 3) // green
		BSP_LCD_DrawRect(154, 59,  17, 17);
		
    // ADDING BORDER FOR TANK 2 CHOOSEN COLOR
	if (tankTwoColor == 1) // red
		BSP_LCD_DrawRect(74,  149, 17, 17);
	else if (tankTwoColor == 2) // blue
		BSP_LCD_DrawRect(114, 149, 17, 17);
    else if (tankTwoColor == 3) // green
		BSP_LCD_DrawRect(154, 149, 17, 17);
    
}

void About() {
    
    // BACKGROUND COLOR
    BSP_LCD_Clear(LCD_COLOR_WHITE); 
    
    // TEXT DRAWING COLOR
    BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
    
    // ABOUT TEXT
    BSP_LCD_DisplayStringAt(0, 10,  (uint8_t *)"Ugradbeni sistemi",     CENTER_MODE);
    BSP_LCD_DisplayStringAt(0, 30,  (uint8_t *)"2021/2022",             CENTER_MODE);
    BSP_LCD_DisplayStringAt(0, 50,  (uint8_t *)"Projekat: Tank Wars",   CENTER_MODE);
    BSP_LCD_DisplayStringAt(0, 70,  (uint8_t *)"Clanovi tima:",         CENTER_MODE);
    BSP_LCD_DisplayStringAt(0, 90,  (uint8_t *)" - Harun Cehajic  ",    CENTER_MODE);
    BSP_LCD_DisplayStringAt(0, 110, (uint8_t *)" - Ermin Jamakovic",    CENTER_MODE);
    BSP_LCD_DisplayStringAt(0, 130, (uint8_t *)" - Aldin Kulaglic ",    CENTER_MODE);
    
    // BACK BUTTON
    BSP_LCD_DisplayStringAt(0, 200, (uint8_t *)"BACK", CENTER_MODE);
}

void RenderScene() {
    
    // BACKGROUND COLOR
    BSP_LCD_Clear(LCD_COLOR_WHITE); 
    
    // TEXT DRAWING COLOR
    BSP_LCD_SetTextColor(LCD_COLOR_DARKGRAY);
    
    // LEFT BARRIER
    BSP_LCD_FillRect(0, 120, 40, 15);
    
    // RIGHT BARRIER
    BSP_LCD_FillRect(200, 120, 40, 15);
    
    // TOP BARRIED
    BSP_LCD_FillRect(113, 0, 15, 40);
    
    // BOTTOM BARRIER
    BSP_LCD_FillRect(113,200, 15, 40);
    
    // MIDDLE BARRIER
    BSP_LCD_FillRect(113, 80, 15, 90); // VERTICAL
    BSP_LCD_FillRect(75,120, 90, 15);      // HORIZONTAL
    
    // TOP LEFT BOX
    //BSP_LCD_FillRect(BSP_LCD_GetXSize() / 4 - 15, BSP_LCD_GetYSize() / 4 - 15, 30, 30);
    
    // BOTTOM RIGHT BOX
    BSP_LCD_FillRect(167, 167, 30, 30);
}
bool igraJeUToku = false;
void PauseMenu() {
    
    // iz PauseMenu-a se moze nastaviti igra ili vratiti u MainMenu
    BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
    BSP_LCD_FillRect(65, 80, 90, 40);
    BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
    BSP_LCD_DisplayStringAt(0, 100, (uint8_t *)"CONTINUE", CENTER_MODE);
    BSP_LCD_DisplayStringAt(0, 120, (uint8_t *)"MAIN MENU", CENTER_MODE);
    igraJeUToku = false;
}

void Pauza() {
    
    activeScreen = 5;
    PauseMenu();
}

Ticker igra, igraMQTT;

void game() {
    
    if (activeScreen != 5) {
        
        BSP_LCD_Clear(LCD_COLOR_WHITE);
        RenderScene();
    	tank1.updateScreen();
    	tank2.updateScreen();
    } 
}

// treba dodat za startanje igre
void NewGame() {
    
    // pokrece instancu igre, tj. postavljanje mape i iscrtavanje prepreka
    RenderScene();
    
    tank1.setColor(tankOneColor);
    tank2.setColor(tankTwoColor);
    
    // Iscrtavanje tenkova na ekran
    tank1.draw();
    tank2.draw();
    
    // Postavljanje moda-aplikacije
    igraJeUToku = true;
    //pauza.rise(&Pauza);
    
    // Glavna funkcija za manipulisanje jedne runde
    igra.attach(&game, 0.7);
}


/*
 Pobjednički ekran nakon što neki igrać izvrši pogodak
*/
void WinningScreen(int color) {
    
    // Reset svih parametara runde
    BSP_LCD_Clear(LCD_COLOR_WHITE);
	igraJeUToku = false;
	igra.detach();
    player1T.detach();
    player2T.detach();
    tank1.reset();
    tank2.reset();
 	BSP_LCD_Clear(LCD_COLOR_WHITE);
   
               
    // Ispis pojednika
	BSP_LCD_DisplayStringAt(0, 80, (uint8_t *)"WINNER IS", CENTER_MODE);
 
	if (color == tankOneColor) {
	    BSP_LCD_DisplayStringAt(0, 100, (uint8_t *)"Player 1", CENTER_MODE);
	} else { 
	    BSP_LCD_DisplayStringAt(0, 100, (uint8_t *)"Player 2", CENTER_MODE);
	}
	BSP_LCD_DisplayStringAt(0, 160, (uint8_t *)"MAIN MENU", CENTER_MODE);
	activeScreen = 6;
	
}


// Menu controller za kretanje kroz aplikaciju
void MenuController(int x, int y) {
	
	switch(activeScreen) {
                
        case 1: { // IF MAIN MENU ACTIVE
                    
            if ((x > 69 && x < 160) && (y > 79 && y < 91)) {
                        
                // STARTING NEW GAME
                activeScreen = 2;
                BSP_LCD_Clear(LCD_COLOR_WHITE);
                NewGame();
            }
            else if ((x > 49 && x < 180) && (y > 99 && y < 114)) {
                        
				// CHANGE COLOR 
                activeScreen = 3;
                BSP_LCD_Clear(LCD_COLOR_WHITE);
                ChangeColor();
            }
            else if ((x > 90 && x < 145) && (y > 119 && y < 132)) {
                        
                // CHANGE MAP
                activeScreen = 4;
                BSP_LCD_Clear(LCD_COLOR_WHITE);
                About();
            }
                    
        } break;
                
        case 2: { // RUNNING GAME
                
            // URADJENO U NewGame()
        } break;
                
        case 3: { // CHANGING COLOR OF TANKS
                    
            if ((x > 76 && x < 93) && (y > 62 && y < 80)) {
                        
                // TANK ONE COLOR RED
                tankOneColor = 1;
                BSP_LCD_Clear(LCD_COLOR_WHITE);
                ChangeColor();
            }
            else if ((x > 115 && x < 130) && (y > 62 && y < 80)) {
                        
                // TANK ONE COLOR BLUE
                tankOneColor = 2;
                BSP_LCD_Clear(LCD_COLOR_WHITE);
                ChangeColor();
            }
            else if ((x > 155 && x < 170) && (y > 62 && y < 80)) {
                        
                // TANK ONE COLOR GREEN
                tankOneColor = 3;
                BSP_LCD_Clear(LCD_COLOR_WHITE);
                ChangeColor();
            }
            else if ((x > 76 && x < 93) && (y > 152 && y < 170)) {
                        
                // TANK TWO COLOR RED
                tankTwoColor = 1;
                BSP_LCD_Clear(LCD_COLOR_WHITE);
                ChangeColor();
            }
            else if ((x > 115 && x < 130) && (y > 152 && y < 170)) {
                        
                // TANK TWO COLOR BLUE
                tankTwoColor = 2;
                BSP_LCD_Clear(LCD_COLOR_WHITE);
                ChangeColor();
            }
            else if ((x > 155 && x < 170) && (y > 152 && y < 170)) {
                        
                // TANK TWO COLOR GREEN
                tankTwoColor = 3;
                BSP_LCD_Clear(LCD_COLOR_WHITE);
                ChangeColor();
            }
            else if ((x > 95 && x < 140) && (y > 200 && y < 215)) {
                        
                // BACK TO MAIN MENU
                BSP_LCD_Clear(LCD_COLOR_WHITE);
                activeScreen = 1;
                MainMenu();
            }
                    
        } break;
                
        case 4: { // ABOUT
                    
            if ((x > 95 && x < 140) && (y > 200 && y < 215)) {
                        
				// BACK TO MAIN MENU
                BSP_LCD_Clear(LCD_COLOR_WHITE);
                activeScreen = 1;
                MainMenu();
             }
        } break;
        
        case 5: { // PAUSE MENU
        
            if ((x > 74 && x < 160) && (y > 100 && y < 115)) {
                
                // CONTINUE GAME
                activeScreen = 2;
                game();
                igraJeUToku = true;
            }
           else if ((x > 65 && x < 165) && (y > 120 && y < 135)) {
               
               // BACK TO MAIN MENU
               igraJeUToku = false;
               igra.detach();
               player1T.detach();
               player2T.detach();
               tank1.reset();
               tank2.reset();
			   BSP_LCD_Clear(LCD_COLOR_WHITE);
               activeScreen = 1;
               MainMenu();
           }
            
        } break;
 
		case 6: {
 
			if ((x > 40 && x < 195) && (y > 164 && y < 180)) {
               
               activeScreen = 1;
               igraJeUToku = false;
               MainMenu();
			}
		} break;
                
    }
}



/*

    Funkcije odgovorne za procesiranje signala dobijenih iz MQTT Dash aplikacije.
    Funkcije će se pozvati prilikom pritiska na odgovarajuće komponente 
    unutar MQTT Dash aplikacije.
    Kada se prosljedi signal 1, funkcije pozivaju odgovarajući ticker za igrača, koji je 
    odgovoran za konstantno pozivanje funkcije za kretanje u tom smjeru. Kada bude
    prosljeđena 0, ticker se detach-a

*/

//  FUNKCIJE OBRADE PRISTIGLIH SIGNALA IZ MQTT-DASH-a ZA IGRACA 1

void messageArrived_up_player1(MQTT::MessageData& md) {
	
    MQTT::Message &message = md.message;
    
    /*printf("Message arrived: qos %d, retained %d, dup %d, packetid %d\r\n", message.qos, message.retained, message.dup, message.id);
    printf("Payload %.*s\r\n", message.payloadlen, (char*)message.payload);
    ++arrivedcount;
    */
    char* str=(char*)message.payload;
    int up_player1=atoi(str);
    
    if(up_player1 == 1){
        player1T.attach_us(Callback<void()>(&tank1, &Tank::moveUp), 0.5e5);
    }else{
        player1T.detach();
    }
}

void messageArrived_down_player1(MQTT::MessageData& md) {
	
     MQTT::Message &message = md.message;
    
    printf("Message arrived: qos %d, retained %d, dup %d, packetid %d\r\n", message.qos, message.retained, message.dup, message.id);
    printf("Payload %.*s\r\n", message.payloadlen, (char*)message.payload);
    ++arrivedcount;

    char* str=(char*)message.payload;
    int down_player1=atoi(str);
    printf("D %d\n", down_player1);
    
    
    if(down_player1 == 1 || down_player1 == 10){
        printf("Dobro\n");
        player1T.attach_us(Callback<void()>(&tank1, &Tank::moveDown),0.5e5);
    }else{
        player1T.detach();
    }
}

void messageArrived_left_player1(MQTT::MessageData& md) {
	
     MQTT::Message &message = md.message;
    /*
    printf("Message arrived: qos %d, retained %d, dup %d, packetid %d\r\n", message.qos, message.retained, message.dup, message.id);
    printf("Payload %.*s\r\n", message.payloadlen, (char*)message.payload);
    ++arrivedcount;
    */
    char* str=(char*)message.payload;
    int left_player1=atoi(str);
    
    if(left_player1 == 1 || left_player1 == 10){
        player1T.attach_us(Callback<void()>(&tank1, &Tank::moveLeft), 0.5e5);
    }else{
        player1T.detach();
    }
}

void messageArrived_right_player1(MQTT::MessageData& md) {
	
    MQTT::Message &message = md.message;
    /*
    printf("Message arrived: qos %d, retained %d, dup %d, packetid %d\r\n", message.qos, message.retained, message.dup, message.id);
    printf("Payload %.*s\r\n", message.payloadlen, (char*)message.payload);
    ++arrivedcount;
    */
    char* str=(char*)message.payload;
    int right_player1=atoi(str);
    
    if(right_player1 == 1){
        player1T.attach_us(Callback<void()>(&tank1, &Tank::moveRight), 0.5e5);
    }else{
        player1T.detach();
    }
}

void messageArrived_shoot_player1(MQTT::MessageData& md) {
	
     MQTT::Message &message = md.message;
    /*
    printf("Message arrived: qos %d, retained %d, dup %d, packetid %d\r\n", message.qos, message.retained, message.dup, message.id);
    printf("Payload %.*s\r\n", message.payloadlen, (char*)message.payload);
    ++arrivedcount;
    */
    char* str=(char*)message.payload;
    int shoot_player1=atoi(str);
    
    if(shoot_player1 == 1){
        tank1.shoot();
    }
}


// FUNKCIJE OBRADE PRISTIGLIH SIGNALA IZ MQTT-DASH-a ZA IGRACA 2

void messageArrived_up_player2(MQTT::MessageData& md) {
	
    MQTT::Message &message = md.message;
    /*
    printf("Message arrived: qos %d, retained %d, dup %d, packetid %d\r\n", message.qos, message.retained, message.dup, message.id);
    printf("Payload %.*s\r\n", message.payloadlen, (char*)message.payload);
    ++arrivedcount;
    */
    char* str=(char*)message.payload;
    int up_player2=atoi(str);
    
    if(up_player2 == 1){
        player2T.attach_us(Callback<void()>(&tank2, &Tank::moveUp), 0.5e5);
    }else{
        player2T.detach();
    }
}

void messageArrived_down_player2(MQTT::MessageData& md) {
	
     MQTT::Message &message = md.message;
    /*
    printf("Message arrived: qos %d, retained %d, dup %d, packetid %d\r\n", message.qos, message.retained, message.dup, message.id);
    printf("Payload %.*s\r\n", message.payloadlen, (char*)message.payload);
    ++arrivedcount;
    */
    char* str=(char*)message.payload;
    int down_player2=atoi(str);
    
    if(down_player2 == 1 || down_player2 == 10){
        player2T.attach_us(Callback<void()>(&tank2, &Tank::moveDown), 0.5e5);
    }else{
        player2T.detach();
    }
}

void messageArrived_left_player2(MQTT::MessageData& md) {
	
    MQTT::Message &message = md.message;
    /*
    printf("Message arrived: qos %d, retained %d, dup %d, packetid %d\r\n", message.qos, message.retained, message.dup, message.id);
    printf("Payload %.*s\r\n", message.payloadlen, (char*)message.payload);
    ++arrivedcount;
    */
    char* str=(char*)message.payload;
    int left_player2=atoi(str);
    
    if(left_player2 == 1 || left_player2 == 10){
        player2T.attach_us(Callback<void()>(&tank2, &Tank::moveLeft), 0.5e5);
    }else{
        player2T.detach();
    }
}

void messageArrived_right_player2(MQTT::MessageData& md) {
	
    MQTT::Message &message = md.message;
    /*
    printf("Message arrived: qos %d, retained %d, dup %d, packetid %d\r\n", message.qos, message.retained, message.dup, message.id);
    printf("Payload %.*s\r\n", message.payloadlen, (char*)message.payload);
    ++arrivedcount;
    */
    char* str=(char*)message.payload;
    int right_player2=atoi(str);
    
    if(right_player2 == 1){
        player2T.attach_us(Callback<void()>(&tank2, &Tank::moveRight), 0.5e5);
    }else{
        player2T.detach();
    }
}

void messageArrived_shoot_player2(MQTT::MessageData& md) {
	
      MQTT::Message &message = md.message;
    /*
    printf("Message arrived: qos %d, retained %d, dup %d, packetid %d\r\n", message.qos, message.retained, message.dup, message.id);
    printf("Payload %.*s\r\n", message.payloadlen, (char*)message.payload);
    ++arrivedcount;
    */
    char* str=(char*)message.payload;
    int shoot_player2=atoi(str);
    
    if(shoot_player2 == 1){
        tank2.shoot();
    }
}


// FUNKCIJA ZA PAUZU

void messageArrived_pause(MQTT::MessageData& md) {
	
    MQTT::Message &message = md.message;
    /*
    printf("Message arrived: qos %d, retained %d, dup %d, packetid %d\r\n", message.qos, message.retained, message.dup, message.id);
    printf("Payload %.*s\r\n", message.payloadlen, (char*)message.payload);
    ++arrivedcountClient2;
    */
    char* str=(char*)message.payload;
    int pas=atoi(str);
    
    if(pas == 1){
        Pauza();
    }
}



int main() {
    
    // INICIJALIZACIJA EKRANA
    printf("Welcome to Tank Wars!\n");
    BSP_LCD_Init();
	printf("Sirina: %d\nVisina: %d\n", BSP_LCD_GetXSize(), BSP_LCD_GetYSize());
    
	//---------------------------------------------DIO ZA KOMUNIKACIJU----------------------------------------------------------------------------
	
	SocketAddress a;
    
    NetworkInterface *network, *network_client2, *network_client3;
    network = NetworkInterface::get_default_instance();
    network_client2 = NetworkInterface::get_default_instance();
    network_client3 = NetworkInterface::get_default_instance();
   
    if (!network || !network_client2 || !network_client3) return -1;    
    
    MQTTNetwork mqttNetwork(network);
    MQTTNetwork mqttNetworkClient2(network_client2);
    MQTTNetwork mqttNetworkClient3(network_client3);

    MQTT::Client<MQTTNetwork, Countdown> client(mqttNetwork);
    MQTT::Client<MQTTNetwork, Countdown> client2(mqttNetworkClient2);
    MQTT::Client<MQTTNetwork, Countdown> client3(mqttNetworkClient3);
    
    const char* hostname = "broker.hivemq.com";
    int port = 1883;
    printf("Connecting to %s:%d\r\n", hostname, port);
    int rc = mqttNetwork.connect(hostname, port);
    if (rc != 0) printf("rc from TCP connect is %d\r\n", rc);
    else printf("Connected to broker!\r\n"); 
        
    printf("Connecting to %s:%d\r\n", hostname, port);
    rc = mqttNetworkClient2.connect(hostname, port);
    if (rc != 0) printf("rc from TCP connect is %d\r\n", rc);
    else printf("Connected to broker!\r\n");
        
        
    printf("Connecting to %s:%d\r\n", hostname, port);
    rc = mqttNetworkClient3.connect(hostname, port);
    if (rc != 0) printf("rc from TCP connect is %d\r\n", rc);
    else printf("Connected to broker!\r\n");
        
        
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    data.clientID.cstring = MQTT_CLIENT_NAME;
    data.username.cstring = "";
    data.password.cstring = "";
    
    MQTTPacket_connectData dataClient2 = MQTTPacket_connectData_initializer;
    dataClient2.MQTTVersion = 3;
    dataClient2.clientID.cstring = MQTT_CLIENT2_NAME;
    dataClient2.username.cstring = "";
    dataClient2.password.cstring = "";
    
    MQTTPacket_connectData dataClient3 = MQTTPacket_connectData_initializer;
    dataClient2.MQTTVersion = 3;
    dataClient2.clientID.cstring = MQTT_CLIENT3_NAME;
    dataClient2.username.cstring = "";
    dataClient2.password.cstring = "";
    
    
    
    
    
    if ((rc = client.connect(data)) != 0)
        printf("rc from MQTT connect is %d\r\n", rc);
        
    if ((rc = client2.connect(dataClient2)) != 0)
        printf("rc from MQTT connect is %d\r\n", rc);
        
     if ((rc = client3.connect(dataClient3)) != 0)
        printf("rc from MQTT connect is %d\r\n", rc);
  
        
        
    if ((rc = client.subscribe(TEMASUBUPPLAYER1, MQTT::QOS0, messageArrived_up_player1)) != 0)
        printf("rc from MQTT subscribe is %d\r\n", rc);
    else
        printf("Subscribed to %s\r\n", TEMASUBUPPLAYER1);
        
     if ((rc = client.subscribe(TEMASUBDOWNPLAYER1, MQTT::QOS0, messageArrived_down_player1)) != 0)
        printf("rc from MQTT subscribe is %d\r\n", rc);
    else
        printf("Subscribed to %s\r\n", TEMASUBDOWNPLAYER1);
       
     if ((rc = client.subscribe(TEMASUBRIGHTPLAYER1, MQTT::QOS0, messageArrived_right_player1)) != 0)
        printf("rc from MQTT subscribe is %d\r\n", rc);
    else
        printf("Subscribed to %s\r\n", TEMASUBRIGHTPLAYER1);

    if ((rc = client.subscribe(TEMASUBLEFTPLAYER1, MQTT::QOS0, messageArrived_left_player1)) != 0)
        printf("rc from MQTT subscribe is %d\r\n", rc);
    else
        printf("Subscribed to %s\r\n", TEMASUBLEFTPLAYER1);
        
     if ((rc = client.subscribe(TEMASUBSHOOTPLAYER1, MQTT::QOS0, messageArrived_shoot_player1)) != 0)
        printf("rc from MQTT subscribe is %d\r\n", rc);
    else
        printf("Subscribed to %s\r\n", TEMASUBSHOOTPLAYER1);
	
	
      
    if ((rc = client2.subscribe(TEMASUBUPPLAYER2, MQTT::QOS0, messageArrived_up_player2)) != 0)
        printf("rc from MQTT subscribe is %d\r\n", rc);
    else
        printf("Subscribed to %s\r\n", TEMASUBUPPLAYER2);
        
    if ((rc = client2.subscribe(TEMASUBDOWNPLAYER2, MQTT::QOS0, messageArrived_down_player2)) != 0)
        printf("rc from MQTT subscribe is %d\r\n", rc);
    else
        printf("Subscribed to %s\r\n", TEMASUBDOWNPLAYER2);
       
    if ((rc = client2.subscribe(TEMASUBRIGHTPLAYER2, MQTT::QOS0, messageArrived_right_player2)) != 0)
        printf("rc from MQTT subscribe is %d\r\n", rc);
    else
        printf("Subscribed to %s\r\n", TEMASUBRIGHTPLAYER2);



    if ((rc = client2.subscribe(TEMASUBLEFTPLAYER2, MQTT::QOS0, messageArrived_left_player2)) != 0)
        printf("rc from MQTT subscribe is %d\r\n", rc);
    else
        printf("Subscribed to %s\r\n", TEMASUBLEFTPLAYER2);
        
    if ((rc = client2.subscribe(TEMASUBSHOOTPLAYER2, MQTT::QOS0, messageArrived_shoot_player2)) != 0)
        printf("rc from MQTT subscribe is %d\r\n", rc);
    else
        printf("Subscribed to %s\r\n", TEMASUBSHOOTPLAYER2);
	
	
 
    if ((rc = client3.subscribe(TEMASUBPAUSE, MQTT::QOS0, messageArrived_pause)) != 0)
        printf("rc from MQTT subscribe is %d\r\n", rc);
    else
        printf("Subscribed to %s\r\n", TEMASUBPAUSE);
	
	//---------------------------------------------DIO ZA IGRU------------------------------------------------------------------------------------
	MainMenu();  
    
    // MENU LOOP
    while (1) {
		if(igraJeUToku == false){
		
		//Stanje aplikacije ako igra trenutno nije u toku    
		
        BSP_TS_GetState(&TS_State);
		
        if(TS_State.touchDetected) {
			
            uint16_t x = TS_State.touchX[0];
            uint16_t y = TS_State.touchY[0];
			
            printf("x = %d  y = %d\n", x, y);
			
            MenuController(x, y);
			
            wait_ms(10);
        }
		    
		}
		else{
			/*
			Real-Time provjera podataka iz MQTT-Dash-a. Glavna while peljta prelazi u ovaj režim 
			rada kada se zapoćne nova igra.
			*/  
			//printf("ide\n");
			rc = client.subscribe(TEMASUBUPPLAYER1, MQTT::QOS0, messageArrived_up_player1);
			rc = client.subscribe(TEMASUBDOWNPLAYER1, MQTT::QOS0, messageArrived_down_player1);
			rc = client.subscribe(TEMASUBLEFTPLAYER1, MQTT::QOS0, messageArrived_left_player1);
			rc = client.subscribe(TEMASUBRIGHTPLAYER1, MQTT::QOS0, messageArrived_right_player1);
			rc = client.subscribe(TEMASUBSHOOTPLAYER1, MQTT::QOS0, messageArrived_shoot_player1);
		   
			rc = client2.subscribe(TEMASUBUPPLAYER2, MQTT::QOS0, messageArrived_up_player2);
			rc = client2.subscribe(TEMASUBDOWNPLAYER2, MQTT::QOS0, messageArrived_down_player2);
			rc = client2.subscribe(TEMASUBLEFTPLAYER2, MQTT::QOS0, messageArrived_left_player2);
			rc = client2.subscribe(TEMASUBRIGHTPLAYER2, MQTT::QOS0, messageArrived_right_player2);
			rc = client2.subscribe(TEMASUBSHOOTPLAYER2, MQTT::QOS0, messageArrived_shoot_player2);
		   
			rc = client3.subscribe(TEMASUBPAUSE, MQTT::QOS0, messageArrived_pause);
		}
    }
}