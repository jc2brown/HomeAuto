#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <curl/curl.h>
#include <math.h>
#include <stdarg.h>

#include <bcm2835.h>

#include "rs232.c"
#include "qshttpd.h"

#define DEBUG 0

#if DEBUG
#define dbgprintf printf
#else
#define dbgprintf(fmt, ...) 
#endif


#define MAINS	RPI_GPIO_P1_07
#define RF 		RPI_GPIO_P1_11
#define DTR		RPI_GPIO_P1_12


#define PIN_WRITE(pin,val) 	bcm2835_gpio_write(pin, val) 

#define PIN_READ(pin)		bcm2835_gpio_lev(pin) 

#define PIN_IN(pin) 		bcm2835_gpio_fsel(pin, BCM2835_GPIO_FSEL_INPT)
#define PIN_OUT(pin) 		bcm2835_gpio_fsel(pin, BCM2835_GPIO_FSEL_OUTP)

#define RF_LOCK_PATH "rf.lock"

#define ALARM_PATH "alarm.conf"


CURL *curl;
		
		
void set_handler(Request *request, Response *response);
	
int file_exist (char *filename) {
  struct stat buffer;   
  return (stat (filename, &buffer) == 0);
}

void tone(double freq, double dur) {
	int nPulses = 1 + freq * dur;
	int i;
	for ( i = 0; i < nPulses; ++i ) {
		PIN_WRITE(RF,HIGH);	
		bcm2835_delayMicroseconds(0.5e6 / freq);
		PIN_WRITE(RF,LOW);	
		bcm2835_delayMicroseconds(0.5e6 / freq);
	}
}

void roomlights(int on) {
	dbgprintf("Room Lights: %d\n", on);
	tone( (on ? 80 : 70), .3);
}


void desklights(int on) {
	dbgprintf("Desk Lights: %d\n", on);
	PIN_WRITE(MAINS,(on ? LOW : HIGH));
}


void ledlights(double duty) {
	dbgprintf("LED Lights: %f\n", duty);
	if ( duty > 50.0 ) {
		//duty = 50.0;
	}
	tone(315+duty*120.0/100.0, .3);	
}

int enable_autolights;

void autolights(int on) {
	dbgprintf("Auto Lights: %d\n", on);
	enable_autolights = on;
}



char* display_path = "/dev/ttyAMA0";

time_t last_time = (time_t)NULL;
time_t last_weather_update = (time_t)NULL;

#define SPACES "                                "

void write_buf(FILE *f, char *buf, int len) {
	int i;		
	for ( i = 0; i < len; ++i ) {
		while ( PIN_READ(DTR) != LOW );
		fputc(buf[i], f);
		fflush(f);
		//usleep(10000);
	}	
}


char *timeofday[24] = {
	[0] = "%A past midnight",
	[1 ... 6] = "Early %A morning",
	[7 ... 11] = "%A morning",
	[12 ... 16] = "%A afternoon",
	[17 ... 20] = "%A evening",
	[21 ... 23] = "%A night"
};





#include <sys/sysinfo.h>

#define MIN(x,y) ((x)<(y)?(x):(y))

int read_inner_temperature() {
	            
	            
	//struct sysinfo info;
	//sysinfo(&info);
	//printf("Uptime = %ld\n", info.uptime);

	//char outbuf[4];
	//char inbuf[4];
    //bcm2835_spi_transfernb(outbuf, inbuf, 4);
    //int raw = (inbuf[2] << 8) | inbuf[3];
    
    int temp;
	char outbuf[2] = { 0x00, 0x00 };
	char inbuf[2];
    bcm2835_spi_transfernb(outbuf, inbuf, 2);
    int raw = (inbuf[0] << 8) | (inbuf[1] & 0x0F8);
    
    
    //printf("T_Raw=%04X\n", raw);
    
    //printf("T=%.1f C\n", (raw>>3)*0.0625);
    temp = (int)((raw>>3)*0.0625);
    
    
    // Correct for case temperature rise (crude, but better than nothing)
    
    #define T_RISE 3 // degrees C
    
    temp -= T_RISE;
    
    return temp;
    
    
    //int raw = bcm2835_spi_transfer(0x00);
    //return raw << 1;
    
    
    
    
}


int force_wupdate;
int tempC_inside;

#define WEATHER_MAXLEN 4096
char weather_str[WEATHER_MAXLEN];
		
void update_weather(FILE *f) {	
	FILE *fp = popen("wget --quiet -O - http://weather.noaa.gov/pub/data/observations/metar/decoded/CYKF.TXT","r");
	fread(weather_str, WEATHER_MAXLEN, 1, fp);	
	pclose(fp);		
	tempC_inside = read_inner_temperature();
			
}



char *tempofday[200] = {
	[0 ... 79] = "freezing", // -100 to -21
	[80 ... 89] = "very cold", // -20 to -11
	[90 ... 110] = "cold", // -10 to 10
	[111 ... 116] = "chilly", // 11 to 16
	[117 ... 121] = "cool", // 17 to 21
	[122 ... 127] = "warm", // 22 to 27
	[128 ... 133] = "hot", // 28 to 33
	[134 ... 199] = "very hot", // 34 to 99
};



int print_time(FILE* f) {
	int i;
	char str[1024];
	int e;
	char buf[33];
	time_t t = time(NULL);
	struct tm *st = localtime(&t);
		
	if ( last_time == (time_t)NULL || last_weather_update == (time_t)NULL ) {
		update_weather(f);
		last_time = t;
		last_weather_update = t;
	}
		
	if ( difftime(t, last_time) < 1.0 ) {		
		return -1;
	}
	
	if ( force_wupdate || difftime(t, last_weather_update) > 10*60.0 ) {		
		update_weather(f);
		force_wupdate = 0;
		last_weather_update = t;
	}	
	
	last_time = t;

	int m;	
	strftime(buf, 32, "%H", st);	
	int h = atoi(buf);
	strftime(buf, 32, timeofday[h], st);
	char timeish[256];
	strcpy(timeish, buf);
	

	strftime(buf, 32, "%-I:%M %p  %-d %B %Y", st);	
	char clocktime[256];
	strcpy(clocktime, buf);
	
	strftime(buf, 32, "%H", st);	
	
	h = atoi(buf);	
	
	strftime(buf, 32, "%M", st);	
	m = atoi(buf);
	
	char condS[256] = { 0 };
	char *cs = condS;	
		
	char *temp_str = strstr(weather_str, "Weather: ");
	if ( temp_str != NULL ) {
		temp_str += strlen( "Weather: ");
		i = 0;
		if ( !strncmp(temp_str, "thunder in the vicinity", 20) ) {
			strcpy(temp_str, "thunder nearby\n");
			//printf("^^^ %s\n", temp_str);
		}
		while ( *temp_str != '\0' && *temp_str != '\n') {
			//printf("%c\n", *temp_str);
			*cs++ = *temp_str++;
			//++i;
		}
		*cs = '\0';
	} else {
		strcpy(cs, "Calm");
	}
	
	condS[0] = toupper(condS[0]);
	
		
	temp_str = strstr(weather_str, "Sky conditions: ") + strlen("Sky conditions: ");
	char skyS[4096];
	char *ss = skyS;
	while ( ( *ss++ = *temp_str++ ) != '\n' );
	*(--ss) = '\0';
	

	tempC_inside = (read_inner_temperature() + 7*tempC_inside) / 8;
	sprintf(buf, "%d\xF8\x43 in here", tempC_inside);
	
	int tempF, tempC = -99;
	temp_str = strstr(weather_str, "Temperature: ");
	sscanf(temp_str, "Temperature: %d F (%d C)\n", &tempF, &tempC);	
		
		
		
	
	char weatherish[256];
	sprintf(weatherish, "%s & %s", condS, skyS);
		
	char therm[256];
	sprintf(therm, "%d\xF8\x43 in here, %d\xF8\x43 out there", tempC_inside, tempC);
	
	char line1[256];
	sprintf(line1, "%-27s %35s\n", timeish, weatherish);
		
	char line2[256];
	sprintf(line2, "%-31s %31s\n", clocktime, therm);
	
	char* s = 
	"\x1f\x28\x67\x01\x02"   	// Set large font
	"\x1f\x28\x67\x41\x01"  	// Disable bold
	"\x1f\x28\x67\x01\x02"// Set medium font
	"\x1f\x24\x00\x00\x00\x00"; 	// Set cursor 0,0	
		   	
		   	
	
	write_buf(f, s, 21);	
	
	write_buf(f, line1, strlen(line1));	
	write_buf(f, line2, strlen(line2));		
	
	return h*60+m;
}


FILE *display;


#define CHROOT_ITEMS_PATH "homeauto.conf"
#define FULL_ITEMS_PATH CHROOT_ITEMS_PATH


char *items_path = FULL_ITEMS_PATH;


enum ItemType_enum {
	ItemType_SWITCH,
	ItemType_SLIDER,
	ItemType_BUTTON,
	N_ItemTypes
};
typedef enum ItemType_enum ItemType;



struct Switch_struct {
	int value;
};
typedef struct Switch_struct Switch;

struct Slider_struct {
	int min;
	int max;
	int value;
};
typedef struct Slider_struct Slider;

struct Button_struct {
	char *label;
};
typedef struct Button_struct Button;


struct Item_struct {
	int id;
	ItemType type;
	char *handler;
	char *name;
	void *details;	
};
typedef struct Item_struct Item;



void print_switch(Item *item) {
	Switch *switchDetails = (Switch *)item->details;
	dbgprintf("Switch: id=%d handler=%s value=%d\n", item->id, item->handler, switchDetails->value);
}

void print_slider(Item *item) {
	Slider *sliderDetails = (Slider *)item->details;
	dbgprintf("Slider: id=%d handler=%s min=%d max=%d value=%d\n", item->id, item->handler, sliderDetails->min, sliderDetails->max, sliderDetails->value);
}

void print_button(Item *item) {
	Button *buttonDetails = (Button *)item->details;
	dbgprintf("Button: id=%d handler=%s value=%s\n", item->id, item->handler, buttonDetails->label);
}

void print_item(Item *item) {
	switch ( item->type ) {
		case ItemType_SWITCH:
			print_switch(item);
			break;
		case ItemType_SLIDER:
			print_slider(item);
			break;
		case ItemType_BUTTON:
			print_button(item);
			break;
	}
}




Item *create_item(ItemType type, int id, char *handler, char *name, void *details) {
	Item *item = (Item *)malloc(sizeof(Item));
	item->id = id;
	item->type = type;
	item->handler = (char *)malloc(256);
	strcpy(item->handler, handler);
	item->name = (char *)malloc(256);
	strcpy(item->name, name);
	item->details = details;
	return item;
}


Item *create_switch(int id, char *handler, char *name, int value) {
	Switch *switchDetails = (Switch *)malloc(sizeof(Switch));
	switchDetails->value = value;
	Item *item = create_item(ItemType_SWITCH, id, handler, name, switchDetails);
	return item;
}

Item *create_slider(int id, char *handler, char *name, int min, int max, int value) {
	Slider *sliderDetails = (Slider *)malloc(sizeof(Slider));
	sliderDetails->min = min;
	sliderDetails->max = max;
	sliderDetails->value = value;
	Item *item = create_item(ItemType_SLIDER, id, handler, name, sliderDetails);
	return item;
}

Item *create_button(int id, char *handler, char *name, char *label) {
	Button *buttonDetails = (Button *)malloc(sizeof(Button));
	buttonDetails->label = (char *)malloc(256);
	strcpy(buttonDetails->label, label);
	Item *item = create_item(ItemType_BUTTON, id, handler, name, buttonDetails);
	return item;
}



void set_switch_value(Item *item, int value) {	
	Switch *switchDetails = (Switch *)(item->details);
	switchDetails->value = value;
	dbgprintf("switch=%d\n", switchDetails->value);
}

void set_slider_value(Item *item, int value) {	
	Slider *sliderDetails = (Slider *)item->details;
	sliderDetails->value = value;
	dbgprintf("slider=%d\n", sliderDetails->value);
}

void set_button_value(Item *item) {	
	dbgprintf("button\n");
}

Item *get_item_by_handler(char *handler, Item **items, int n_items) {
	int i;
	for ( i = 0; i < n_items; ++i ) {
		if ( ! strcmp(items[i]->handler, handler) ) {
			return items[i];
		}
	}
	return NULL;
}

Item *get_item_by_id(int id, Item **items, int n_items) {
	int i;
	for ( i = 0; i < n_items; ++i ) {
		if ( items[i]->id == id ) {
			return items[i];
		}
	}
	return NULL;
}


void add_item(Item *item, Item **items, int *n_items) {
	items[*n_items] = item;
	*n_items = *n_items + 1;
}


// id switch handler "Name With Spaces" value
// id slider handler "Name With Spaces" min max value
// id button handler "Name With Spaces" "Label With Spaces"
Item *parse_item(char *line) {	
	int i;	
	char *c = line;	
	
	// Read ID, trim leading whitespace
	char idStr[256];
	i = 0;
	while ( isspace(*c) ) c++;
	while ( isdigit(*c) ) idStr[i++] = *c++;	
	idStr[i] = '\0';
	int id = atoi(idStr);
	// Read type, trim leading whitespace
	char typeStr[256];
	i = 0;
	while ( isspace(*c) ) c++;
	while ( ! isspace(*c) ) typeStr[i++] = *c++;
	typeStr[i] = '\0';
	// Read handler, trim leading whitespace
	char handlerStr[256];
	i = 0;
	while ( isspace(*c) ) c++;
	while ( ! isspace(*c) ) handlerStr[i++] = *c++;
	handlerStr[i] = '\0';
	// Read quote-surrounded name, trim leading whitespace
	char nameStr[256];
	i = 0;
	while ( isspace(*c) ) c++;
	while ( *c != '"' ) c++;
	c++;
	while ( *c != '"' ) nameStr[i++] = *c++;
	c++;
	nameStr[i] = '\0';
			
		
	if ( ! strcmp(typeStr, "switch") ) {
		// Read value, trim leading whitespace
		char valueStr[256];
		i = 0;
		while ( isspace(*c) ) c++;
		while ( isdigit(*c) ) valueStr[i++] = *c++;
		valueStr[i] = '\0';
		return create_switch(id, handlerStr, nameStr, atoi(valueStr));
				
	} else if ( ! strcmp(typeStr, "slider") ) {
		// Read min, trim leading whitespace
		char minStr[256];
		i = 0;
		while ( isspace(*c) ) c++;
		while ( isdigit(*c) ) minStr[i++] = *c++;
		minStr[i] = '\0';
		// Read max, trim leading whitespace
		char maxStr[256];
		i = 0;
		while ( isspace(*c) ) c++;
		while ( isdigit(*c) ) maxStr[i++] = *c++;
		maxStr[i] = '\0';
		// Read value, trim leading whitespace
		char valueStr[256];
		i = 0;
		while ( isspace(*c) ) c++;
		while ( isdigit(*c) ) valueStr[i++] = *c++;
		valueStr[i] = '\0';
		return create_slider(id, handlerStr, nameStr, atoi(minStr), atoi(maxStr), atoi(valueStr));
		
	} else if ( ! strcmp(typeStr, "button") ) {
		// Read quote-surrounded label, trim leading whitespace
		char labelStr[256];
		i = 0;
		while ( isspace(*c) ) c++;
		while ( *c != '"' ) c++;
		c++;
		while ( *c != '"' ) labelStr[i++] = *c++;
		c++;
		labelStr[i] = '\0';
		return create_button(id, handlerStr, nameStr, labelStr);
	} 
	
	return NULL;
}


Item **load_items(char *path, int *n_items) {	
	
	Item **items = NULL;
	
	char *line;
	int len;
	int n;
		
	FILE *f = fopen(path, "r");
	if ( f == NULL ) {
		dbgprintf("load_items: could not open file %s\n", path);
		return;
	}
	
	*n_items = 0;
	
	while ( ! feof(f) ) {		
		n = 0;
		len = getline(&line, &n, f);		
		if ( len > 1 ) {
			items = realloc(items, (*n_items + 1) * sizeof(Item*));
			
			add_item(parse_item(line), items, n_items);
			
		}
		free(line);	
		line = NULL;
	}
	
	fclose(f);
	return items;
}


void tostr_item(Item *item, char *line, int *n) {
	
	strcpy(line, "");
	
	char idStr[256];
	sprintf(idStr, "%d ", item->id);
	strcat(line, idStr);
	
	
	char typeStr[256];
	switch ( item->type ) {
		case ItemType_SWITCH:			
			sprintf(typeStr, "%s ", "switch");
			break;
		case ItemType_SLIDER:		
			sprintf(typeStr, "%s ", "slider");
			break;
		case ItemType_BUTTON:		
			sprintf(typeStr, "%s ", "button");
			break;
	}
	strcat(line, typeStr);	
	
	
	char handlerStr[256];
	sprintf(handlerStr, "%s ", item->handler);
	strcat(line, handlerStr);
	
	char nameStr[256];
	sprintf(nameStr, "\"%s\" ", item->name);
	strcat(line, nameStr);
		
	char valueStr[256];
	char minStr[256];	
	char maxStr[256];
	char labelStr[256];
	
	switch ( item->type ) {
		case ItemType_SWITCH:			
			sprintf(valueStr, "%d ", ((Switch *)(item->details))->value);
			strcat(line, valueStr);			
			break;
		case ItemType_SLIDER:		
			sprintf(minStr, "%d ", ((Slider *)(item->details))->min);
			strcat(line, minStr);	
			sprintf(maxStr, "%d ", ((Slider *)(item->details))->max);
			strcat(line, maxStr);		
			sprintf(valueStr, "%d ", ((Slider *)(item->details))->value);	
			strcat(line, valueStr);		
			break;
		case ItemType_BUTTON:		
			sprintf(labelStr, "\"%s\" ", ((Button *)(item->details))->label);	
			strcat(line, labelStr);	
			break;
	}
	*n = strlen(line);
}

void save_items(char *path, Item **items, int n_items) {	
	
	char line[1024];
	int len;
	int n;
	int i;
		
	FILE *f = fopen(path, "w+");
	if ( f == NULL ) {
		dbgprintf("save_items: could not open file %s\n", path);
		return;
	}
	
	for ( i = 0; i < n_items; ++i ) {	
		tostr_item(items[i], line, &n);
		fprintf(f, "%s\n", line);		
	}
	
	fclose(f);
}


void free_switch(Item *item) {
	Switch *switchDetails = (Switch *)item->details;
	free(switchDetails);
}

void free_slider(Item *item) {
	Slider *sliderDetails = (Slider *)item->details;
	free(sliderDetails);
}

void free_button(Item *item) {
	Button *buttonDetails = (Button *)item->details;
	free(buttonDetails->label);
	free(buttonDetails);
}


void free_item(Item *item) {
	switch ( item->type ) {
		case ItemType_SWITCH:
			free_switch(item);
			break;
		case ItemType_SLIDER:
			free_slider(item);
			break;
		case ItemType_BUTTON:
			free_button(item);
			break;
	}
	free(item->handler);
	free(item->name);
	free(item);
}


void free_items(Item **items, int n_items) {
	int i;
	Item *item;
	for ( i = 0; i < n_items; ++i ) { 
		item = items[i];
		free_item(item);
	}
}


void append_load_switch(char *str, Item *item) {	
	Switch *switchDetails = (Switch *)item->details;	
	sprintf(str, "%s [ \"switch\", %d, \"%s\", \"%s\", %d ], \n", 
		str, item->id, "set", item->name, switchDetails->value);	
}

void append_load_slider(char *str, Item *item) {	
	Slider *sliderDetails = (Slider *)item->details;
	sprintf(str, "%s [ \"slider\", %d, \"%s\", \"%s\", %d, %d, %d ], \n", 
		str, item->id, "set", item->name, sliderDetails->min, sliderDetails->max, sliderDetails->value );	
}

void append_load_button(char *str, Item *item) {	
	Button *buttonDetails = (Button *)item->details;
	sprintf(str, "%s [ \"button\", %d, \"%s\", \"%s\", \"%s\" ], \n", 
		str, item->id, "set", item->name, buttonDetails->label);	
}
	

void append_load_item(char *str, Item *item) {
	switch ( item->type ) {
		case ItemType_SWITCH:
			append_load_switch(str, item);
			break;
		case ItemType_SLIDER:
			append_load_slider(str, item);
			break;
		case ItemType_BUTTON:
			append_load_button(str, item);
			break;
	}
}


void load_handler(Request *request, Response *response) {
	int i;
	int n_items;
	Item **items = load_items(items_path, &n_items);
	dbgprintf("load\n");
	char *param_start = strchr(request->get, '?');
	if ( param_start != NULL ) {
		++param_start;	
		strcpy(response->content, "");
		strcat(response->content, "[ \n");	
		for ( i = 0; i < n_items; ++i ) {
			append_load_item(response->content, items[i]);
		}
		sprintf(response->content, "%s [] ", response->content);	
		strcat(response->content, "]");	
	}
	response->code = 200;
	strcpy(response->mime, "text/javascript");
				
	free_items(items, n_items);
}



void append_update_switch(char *str, int id, int val) {	
	sprintf(str, "%s [ \"switch\", %d, %d ], \n", str, id, val);	
}

void append_update_slider(char *str, int id, int val) {	
	sprintf(str, "%s [ \"slider\", %d, %d ], \n", str, id, val);	
}

void append_update_button(char *str, int id, char *label) {	
	sprintf(str, "%s [ \"button\", %d, \"%s\" ], \n", str, id, label);	
}
		

void update_handler(Request *request, Response *response) {
	int i;
	Item *item;
	int n_items;
	Item **items = load_items(items_path, &n_items);
	dbgprintf("update\n");
	char *param_start = strchr(request->get, '?');
	if ( param_start != NULL ) {
		++param_start;	
		strcpy(response->content, "");
		strcat(response->content, "[ \n");	
								
		
		for ( i = 0; i < n_items; ++i ) {	
			item = items[i];
			switch ( item->type ) {
				case ItemType_SWITCH:
					append_update_switch(response->content, item->id, ((Switch *)(item->details))->value);
					break;
				case ItemType_SLIDER:
					append_update_slider(response->content, item->id, ((Slider *)(item->details))->value);
					break;
				case ItemType_BUTTON:
					append_update_button(response->content, item->id, ((Button *)(item->details))->label);
					break;
			}
		}
		
		sprintf(response->content, "%s [] ", response->content);	
		strcat(response->content, "]");	
	} 
	response->code = 200;
	strcpy(response->mime, "text/javascript");
	free_items(items, n_items);
				
}


void roomlights_handler(Request *request, Response *response) {
	dbgprintf("roomlights\n");
	char *value_start = strchr(request->get, '=');
	if ( value_start != NULL ) {
		++value_start;
		roomlights( ! strcmp(value_start, "true") );
	}
}

void vfd_brightness(int b) {	
	char br[256];
	sprintf(br, "\x1f\x58\%c", '\x10' + b);
	write_buf(display, br, 3);
}

void vfdbright_handler(Request *request, Response *response) {
	dbgprintf("vfdbright\n");
	char *value_start = strchr(request->get, '=');
	if ( value_start != NULL ) {
		++value_start;
		int val = atoi(value_start);
		vfd_brightness(val);
	}				
}

void vfdclear_handler(Request *request, Response *response) {
	dbgprintf("vfdclear\n");
	char *value_start = strchr(request->get, '=');
	if ( value_start != NULL ) {
		++value_start;
		char br[256];
		sprintf(br, "\f");
		fprintf(display, br);
		fflush(display);
	}				
}


void autolights_handler(Request *request, Response *response) {
	dbgprintf("autolights\n");
	char *value_start = strchr(request->get, '=');
	if ( value_start != NULL ) {
		++value_start;
		autolights( ! strcmp(value_start, "true") );
	}			
}


void ledlights_handler(Request *request, Response *response) {
	while ( file_exist (RF_LOCK_PATH) );
	fclose(fopen(RF_LOCK_PATH, "w"));
	dbgprintf("ledlights\n");
	char *value_start = strchr(request->get, '=');
	if ( value_start != NULL ) {
		++value_start;
		int val = atoi(value_start);
		ledlights((double)val);
	}		
	remove(RF_LOCK_PATH);		
}

void allon_handler(Request *request, Response *response) {
	dbgprintf("allon\n");
	char *value_start = strchr(request->get, '=');
	if ( value_start != NULL ) {
		++value_start;
		
		Request *subrequest = create_request();
		subrequest->get = (char*)malloc(256);
		Response *subresponse = create_response();	
		
		strcpy(subrequest->get, "/set?autolights=false");
		set_handler(subrequest, subresponse);
		
		strcpy(subrequest->get, "/set?vfdbright=8");
		set_handler(subrequest, subresponse);
		
		strcpy(subrequest->get, "/set?monitors=truee");
		set_handler(subrequest, subresponse);
		
		strcpy(subrequest->get, "/set?desklights=true");
		set_handler(subrequest, subresponse);
		
		strcpy(subrequest->get, "/set?roomlights=true");
		set_handler(subrequest, subresponse);		
		
		strcpy(subrequest->get, "/set?ledlights=100");
		set_handler(subrequest, subresponse);			
		
		free(subrequest->get);
		destroy_request(subrequest);
		destroy_response(subresponse);
	}		
}

void alloff_handler(Request *request, Response *response) {
	dbgprintf("alloff\n");
	char *value_start = strchr(request->get, '=');
	if ( value_start != NULL ) {
		++value_start;
		
		Request *subrequest = create_request();
		subrequest->get = (char*)malloc(256);
		Response *subresponse = create_response();	
		
		
		
		strcpy(subrequest->get, "/set?autolights=false");
		set_handler(subrequest, subresponse);
	
		strcpy(subrequest->get, "/set?vfdbright=0");
		set_handler(subrequest, subresponse);
		
		strcpy(subrequest->get, "/set?minimize=true");
		set_handler(subrequest, subresponse);
		
		strcpy(subrequest->get, "/set?monitors=false");
		set_handler(subrequest, subresponse);	
	
		strcpy(subrequest->get, "/set?desklights=false");
		set_handler(subrequest, subresponse);
		
		strcpy(subrequest->get, "/set?roomlights=false");
		set_handler(subrequest, subresponse);
		
		strcpy(subrequest->get, "/set?ledlights=0");
		set_handler(subrequest, subresponse);
		
		free(subrequest->get);
		destroy_request(subrequest);
		destroy_response(subresponse);
	}		
}



void midlight_handler(Request *request, Response *response) {
	dbgprintf("midlight\n");
	char *value_start = strchr(request->get, '=');
	if ( value_start != NULL ) {
		++value_start;
		
		Request *subrequest = create_request();
		subrequest->get = (char*)malloc(256);
		Response *subresponse = create_response();	
		
		strcpy(subrequest->get, "/set?autolights=false");
		set_handler(subrequest, subresponse);
	
		strcpy(subrequest->get, "/set?vfdbright=4");
		set_handler(subrequest, subresponse);
	
		strcpy(subrequest->get, "/set?desklights=true");
		set_handler(subrequest, subresponse);	
		
		strcpy(subrequest->get, "/set?roomlights=false");
		set_handler(subrequest, subresponse);
		
		strcpy(subrequest->get, "/set?ledlights=35");
		set_handler(subrequest, subresponse);
		
		free(subrequest->get);
		destroy_request(subrequest);
		destroy_response(subresponse);
	}		
}


void desklights_handler(Request *request, Response *response) {
	dbgprintf("desklights\n");
	char *value_start = strchr(request->get, '=');
	if ( value_start != NULL ) {
		++value_start;
		desklights( ! strcmp(value_start, "true") );
	}
}

void minimize_handler(Request *request, Response *response) {
	dbgprintf("minimize\n");
	char *value_start = strchr(request->get, '=');
	if ( value_start != NULL ) {
		++value_start;		
		CURLcode res;
		if ( curl ) {
			curl_easy_setopt(curl, CURLOPT_URL, "http://192.168.1.10:8081/remote?cmd=exe&code=0x50");
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); 
			res = curl_easy_perform(curl);
			if ( res != CURLE_OK ) {
				fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
			}
		}
	}
}


void monitors_handler(Request *request, Response *response) {
	dbgprintf("monitors\n");
	char *value_start = strchr(request->get, '=');
	if ( value_start != NULL ) {
		++value_start;		
		
		CURLcode res;
		if ( curl ) {
			
			if ( ! strcmp(value_start, "true") ) {
				curl_easy_setopt(curl, CURLOPT_URL, "http://192.168.1.10:8081/remote?cmd=exe&code=0x100");
			} else if ( ! strcmp(value_start, "false") ) {
				curl_easy_setopt(curl, CURLOPT_URL, "http://192.168.1.10:8081/remote?cmd=exe&code=0x1");
			}	
			
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); 
			res = curl_easy_perform(curl);
			if ( res != CURLE_OK ) {
				fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
			}
		}
	}
}


void wupdate_handler(Request *request, Response *response) {
	dbgprintf("wupdate\n");
	force_wupdate = 1;
}


RequestHandler get_handler(char *handler) {
	if ( ! strcmp(handler, "roomlights") ) {
		return roomlights_handler;
	} else if ( ! strcmp(handler, "vfdbright") ) {
		return vfdbright_handler;
	} else if ( ! strcmp(handler, "vfdclear") ) {
		return vfdclear_handler;
	}  else if ( ! strcmp(handler, "ledlights") ) {
		return ledlights_handler;
	}  else if ( ! strcmp(handler, "allon") ) {
		return allon_handler;
	}  else if ( ! strcmp(handler, "alloff") ) {
		return alloff_handler;
	}   else if ( ! strcmp(handler, "midlight") ) {
		return midlight_handler;
	}   else if ( ! strcmp(handler, "desklights") ) {
		return desklights_handler;
	}   else if ( ! strcmp(handler, "minimize") ) {
		return minimize_handler;
	}   else if ( ! strcmp(handler, "monitors") ) {
		return monitors_handler;
	}   else if ( ! strcmp(handler, "autolights") ) {
		return autolights_handler;
	}   else if ( ! strcmp(handler, "wupdate") ) {
		return wupdate_handler;
	}
	return NULL;
}


void set_handler(Request *request, Response *response) {
	dbgprintf("set\n");
	char idStr[256];
	int idLen;
	int id;
	Item *item;
	int n_items;
	RequestHandler handler;
	Item **items = load_items(items_path, &n_items);
	char *param_start = strchr(request->get, '?');
	if ( param_start != NULL ) {
		++param_start;
		char *value_start = strchr(request->get, '=');
		if ( value_start != NULL ) {
			++value_start;
			idLen = (value_start-param_start) - 1;
			strncpy(idStr, param_start, idLen);
			idStr[idLen] = '\0';
			id = atoi(idStr);
			if ( id == 0 ) {				
				item = get_item_by_handler(idStr, items, n_items);		
				dbgprintf("named %s @ %p\n", idStr, item);
			} else {
				item = get_item_by_id(id, items, n_items);			
				dbgprintf("id %d @ %p\n", id, item);		
			}
			if ( item != NULL ) {
				
				if ( handler = get_handler(item->handler) ) {				
					handler(request, response);	
				}
				
				switch ( item->type ) {
					case ItemType_SWITCH:
						set_switch_value(item, !strcmp("true", value_start));
						save_items(items_path, items, n_items);		
						break;
					case ItemType_SLIDER:
						set_slider_value(item, atoi(value_start));
						save_items(items_path, items, n_items);		
						break;
					case ItemType_BUTTON:
						break;
				}			
			}
		}		
	}
		
	response->code = 204;
	//free_items(items, n_items);
}


void tostr_time(char *buf, int h, int m) {	
	sprintf(buf, "%d:%02d", h, m);
}


void parse_time(char *str, int *h, int *m) {
	*h = atoi(str);
	str = strchr(str, ':');
	++str;
	
	*m = atoi(str);
			
	int pm = strchr(str, 'p') || strchr(str, 'P') || (*h) == 12;
				
	
	if ( pm ) {
		if ( (*h) != 12 ) {
			*h += 12;
		}
	} else {
		if ( (*h) == 12 ) {
			*h = 0;
		}
	}
	
	dbgprintf("parse_time -> %d:%d\n", *h, *m);
	
}


void read_time(char *path, int *h, int *m) {
	char *line;
	int n;
	FILE *f = fopen(path, "r");
	int len = getline(&line, &n, f);
	parse_time(line, h, m);
	free(line);
	fclose(f);
}

void write_time(char *path, int h, int m) {
	char line[256];
	FILE *f = fopen(path, "w");
	tostr_time(line, h, m);
	fprintf(f, "%s\n", line);
	fclose(f);
}



void alarm_handler(Request *request, Response *response) {
	dbgprintf("alarm\n");
	char idStr[256];
	int idLen;
	int id;
	RequestHandler handler;
	int h, m;
	char *param_start = strchr(request->get, '?');
	if ( param_start != NULL ) {
		++param_start;
		parse_time(param_start, &h, &m);
		write_time(ALARM_PATH, h, m);
	}
		
	response->code = 204;
	//free_items(items, n_items);
}


void static_handler(Request *request, Response *response) {
	dbgprintf("static\n");
	static_request_handler(request, response);				
}


int homeauto_alive = 1;


void homeauto_sigterm_handler(int s) {
	homeauto_alive = 0;
}


int main(int argc, char* argv[]) {
	    
	    
	Item *item;
	int n_items;
	RequestHandler handler;
	Item **items;
	//int n_items;    
	//Item **items;
	
	
   signal(SIGTERM, homeauto_sigterm_handler);
   signal(SIGKILL, homeauto_sigterm_handler);   
   signal(SIGINT, homeauto_sigterm_handler);   

	
    if ( ! bcm2835_init() ) {
		fprintf(stderr, "Failed to initialize bcm2835 peripherals\n");
        return 1;
	}
	
	
	bcm2835_spi_begin();
    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);      
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);                   
    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_256 ); 
    bcm2835_spi_chipSelect(BCM2835_SPI_CS0);                      
    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);      
    
	
	PIN_OUT(MAINS);	
	PIN_IN(DTR);	
	PIN_OUT(RF);	
	
	    
    //putenv("TZ=EST5EDT,M4.1.0/M10.5.0/2");
    putenv("TZ=:America/Toronto");
    
    display = fopen(display_path, "w+");
        
    if ( display == NULL ) {
		fprintf(stderr, "Failed to open display @ %s\n", display_path);
		return 1;
	}
	
    fprintf(display, "\f\n");
	
		
		
	curl = curl_easy_init();
	
	
	
	
	Request *subrequest = create_request();
	subrequest->get = (char*)malloc(256);
	Response *subresponse = create_response();	

	/*
	strcpy(subrequest->get, "/set?vfdbright=4");
	set_handler(subrequest, subresponse);
	
	strcpy(subrequest->get, "/set?monitors=true");
	set_handler(subrequest, subresponse);	

	strcpy(subrequest->get, "/set?desklights=true");
	set_handler(subrequest, subresponse);	
	
	strcpy(subrequest->get, "/set?roomlights=false");
	set_handler(subrequest, subresponse);
	
	strcpy(subrequest->get, "/set?ledlights=50");
	set_handler(subrequest, subresponse);
	*/
	
	
		
	items_path = FULL_ITEMS_PATH;
	
	items = load_items(items_path, &n_items);	
				
						
	item = get_item_by_handler("roomlights", items, n_items);
	if ( item ) {
		roomlights( ((Switch *)item->details)->value );
	}
	
	item = get_item_by_handler("ledlights", items, n_items);
	if ( item ) {
		//print_item(item);	
		ledlights( ((Slider *)(item->details))->value );
	}
			
	item = get_item_by_handler("desklights", items, n_items);
	if ( item ) {
		desklights( ((Switch *)item->details)->value );
	}
			
	item = get_item_by_handler("vfdbright", items, n_items);
	if ( item ) {
		vfd_brightness( ((Slider *)(item->details))->value );
	}		
				
	item = get_item_by_handler("autolights", items, n_items);
	if ( item ) {
		sprintf(subrequest->get, "/set?autolights=%s", (((Switch *)item->details)->value ? "true" : "false") );
		set_handler(subrequest, subresponse);
	}			
		
	
	free_items(items, n_items);	
		
		
		
	
	
	add_request_handler("/load", load_handler);
	add_request_handler("/update", update_handler);
	add_request_handler("/set", set_handler);
	add_request_handler("/alarm", alarm_handler);
	add_request_handler("/", static_handler);
	
		
	items_path = CHROOT_ITEMS_PATH;
		
	int httpd_pid = start_httpd();
	
	//items_path = FULL_ITEMS_PATH;
	
	
	dbgprintf("QSHTTPD PID = %d\n", httpd_pid);
	
	double t, last_t;
	double lum;
   
  // #define TIME_OFFSET 60* 8+30 
   #define TIME_OFFSET (60* 0+2 )
   #define LUM_OFFSET -0.4
   #define LUM_SCALE 10
   int mod = 0;
	homeauto_alive = 1;
    while ( homeauto_alive ) {		
		
		t = print_time(display);
		//t = 60* 10+00 ;
		
		if ( ! (mod = ((mod + 1) % 10)) ) {
		}
		
		if ( t > 0 && t != last_t ) {
		
			items = load_items(items_path, &n_items);		
			item = get_item_by_handler("autolights", items, n_items);
			enable_autolights = ( (((Switch *)(item->details))->value) ? 1 : 0);
			//dbgprintf("autolights: %d\n", enable_autolights);
			
			free_items(items, n_items);
			
	
			if ( enable_autolights ) {
			
				int h, m;				
				read_time(ALARM_PATH, &h, &m);
			
				dbgprintf("%d:%d\n", h, m);
			
				int time_offset = 60* h + m;
			
			
				lum = 100.0 * (LUM_SCALE * (LUM_OFFSET + 0.5*(1+sin(2*3.141*(t-(time_offset+TIME_OFFSET))/1440))));
								
				if ( lum < 0.0 ) {
					lum = 0.0;
				} else if ( lum > 100.0 ) {
					lum = 100.0;
				}
								
				sprintf(subrequest->get, "/set?ledlights=%d", (int)lum);
				set_handler(subrequest, subresponse);

				sprintf(subrequest->get, "/set?vfdbright=%d", (int)(lum * (8.0/100.0)) );
				set_handler(subrequest, subresponse);
				
				sprintf(subrequest->get, "/set?roomlights=%s", (lum > 95.0 ? "true" : "false"));
				set_handler(subrequest, subresponse);
				
				if ( lum > 95.0 ) {
					//sprintf(subrequest->get, "/set?desklights=true");
					//set_handler(subrequest, subresponse);					
				}
				
				dbgprintf("Time: %d mins  Lum: %d\n", (int)t, (int)lum);
				last_t = t;
			}
		}
		//bcm2835_delayMicroseconds(100e3);
		usleep(1000000);
    }
    
    fprintf(display, "\fSystem stopped\n");
    fflush(display);
    
	free(subrequest->get);
	destroy_request(subrequest);
	destroy_response(subresponse);
	
    kill(httpd_pid, SIGTERM);
    
    
	curl_easy_cleanup(curl);
    
    free_request_handlers();
    
    dbgprintf("HomeAuto done\n");
    
	fclose(display);
	PIN_IN(RF);
	PIN_IN(DTR);
	bcm2835_spi_end();
    bcm2835_close();
    return 0;
}

