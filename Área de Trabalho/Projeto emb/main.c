#include "uart_irda_cir.h"
#include "soc_AM335x.h"
#include "interrupt.h"
#include "board.h"
#include "beaglebone.h"
#include "gpio_v2.h"
#include "consoleUtils.h"
#include "dmtimer.h"

#define		PIN_HIGH   	1
#define     PIN_LOW		0

#define GPIO_INTC_LINE_1                  (0x0)
#define GPIO_INTC_LINE_2                  (0x1)

#define GPIO_INTC_TYPE_NO_LEVEL           (0x01)
#define GPIO_INTC_TYPE_LEVEL_LOW          (0x04)
#define GPIO_INTC_TYPE_LEVEL_HIGH         (0x08)
#define GPIO_INTC_TYPE_BOTH_LEVEL         (0x0C)

#define GPIO_INTC_TYPE_NO_EDGE            (0x80)
#define GPIO_INTC_TYPE_RISE_EDGE          (0x10)
#define GPIO_INTC_TYPE_FALL_EDGE          (0x20)
#define GPIO_INTC_TYPE_BOTH_EDGE          (0x30)

#define T_1MS_COUNT                     (0x5DC0u) 
#define OVERFLOW                        (0xFFFFFFFFu)
#define TIMER_1MS_COUNT                 (0x5DC0u) 

#define ESTADO_INICIAL -1
#define JOGADOR_1 0
#define JOGADOR_2 1

#define TIME_CONT 0x8FFFF

typedef struct{
    int LED_R;
    int LED_G;
    int LED_B;
    int gpio_select;
}LED_RGB;

typedef struct{
    LED_RGB led;
    int pino_saida;
    int gpio_select;
}Botao;

#define LINHAS 3
#define COLUNAS 3

LED_RGB leds[LINHAS * COLUNAS];
Botao botoes[LINHAS * COLUNAS];
int Jogada[LINHAS][COLUNAS];

static void DMTimerSetUp(void);
static void Delay(unsigned int tempo);
static void 		initLed(unsigned int, unsigned int, unsigned int);
static void 		initButton(unsigned int, unsigned int, unsigned int);
static void		gpioAintcConf(void);
static void     initSerial(void);
static unsigned int  	getAddr(unsigned int);
static void 		gpioIsr(void);
static int 		gpioPinIntConf(unsigned int, unsigned int, unsigned int);
static void 		gpioPinIntEnable(unsigned int, unsigned int, unsigned int); 
static void 		gpioIntTypeSet(unsigned int, unsigned int, unsigned int);
static void     InicializaValoresLeds(void);
static void     InicializaValoresBotoes(void);
static void     InicializaLeds(void);
static void     InicializaBotoes(void);
static void     ConfigurarBotoesInterrupcao(void);
static void     ExecutarJogada(unsigned int);
static void     JogadaInvalida(void);
static void     Recolorir(void);
static int      VerificarFimDeJogo(unsigned int);
static int      VerificarLinha(unsigned int);
static int      VerificarColuna(unsigned int);
static int      VerificarDiagonalPrincipal(void);
static int      VerificarDiagonalSecundaria(void);
static void     AcenderLinha(unsigned int);
static void     AcenderColuna(unsigned int);
static void     AcenderDiagonalPrincipal(void);
static void     AcenderDiagonalSecundaria(void);
static void     ResetJogo(void);
static void     InicializaJogo(void);

unsigned int jogada = 0, jogador = 0;
int flag = -1;
unsigned int pont_jogador1 = 0, pont_jogador2 = 0;
char jogador1[50];
char jogador2[50];
volatile unsigned int mSec = 1000;
	
int main(){

    DMTimer2ModuleClkConfig();
    
    initSerial();
    
    IntMasterIRQEnable();

    InicializaValoresLeds();
    InicializaValoresBotoes();
    
    GPIOModuleClkConfig(GPIO0);
    GPIOModuleClkConfig(GPIO1);
    GPIOModuleClkConfig(GPIO2);
    GPIOModuleClkConfig(GPIO3);
    
    InicializaLeds();
    InicializaBotoes();
    
    gpioAintcConf();

    DMTimerSetUp();

    InicializaJogo();    
    
    ConfigurarBotoesInterrupcao();

    while(1);
	
	return(0);
}

static void InicializaJogo(){
    ConsoleUtilsPrintf("\n\n\r ##############################################\n\r");
    ConsoleUtilsPrintf("\r ######  ##  JOGO DA VELHA EMBEDDED   ## ######\n\r");
    ConsoleUtilsPrintf("\r ##############################################\n\r");

    ConsoleUtilsPrintf("\n Digite o nome do jogador 1: ");
    ConsoleUtilsScanf("%s", jogador1);
    ConsoleUtilsPrintf("\n Digite o nome do jogador 2: ");
    ConsoleUtilsScanf("%s", jogador2);
}

static void Delay(unsigned int tempo){
    tempo *= mSec;
    while(tempo != 0){
        DMTimerCounterSet(SOC_DMTIMER_2_REGS, 0);
        DMTimerEnable(SOC_DMTIMER_2_REGS);
        while(DMTimerCounterGet(SOC_DMTIMER_2_REGS) < TIMER_1MS_COUNT);
        DMTimerDisable(SOC_DMTIMER_2_REGS);
        tempo--;
    }
}

static unsigned  int getAddr(unsigned int module){
	unsigned int addr;

	switch (module) {
		case GPIO0:
			addr = SOC_GPIO_0_REGS;	
			break;
		case GPIO1:	
			addr = SOC_GPIO_1_REGS;	
			break;
		case GPIO2:	
			addr = SOC_GPIO_2_REGS;	
			break;
		case GPIO3:	
			addr = SOC_GPIO_3_REGS;	
			break;
		default:	
			break;
	}/* -----  end switch  ----- */

	return(addr);
}

static void initSerial(void){
	ConsoleUtilsInit();
	ConsoleUtilsSetType(CONSOLE_UART);
}

static void initLed(unsigned int baseAdd, unsigned int module, unsigned int pin){
	GPIOPinMuxSetup(module, pin);
	GPIODirModeSet(baseAdd, pin, GPIO_DIR_OUTPUT);
}

static void initButton(unsigned int baseAdd, unsigned int module, unsigned int pin){
    
    	/* Selecting GPIO pin for use. */
    	GPIOPinMuxSetup(module, pin);
    
    	/* Setting the GPIO pin as an output pin. */
    	GPIODirModeSet(baseAdd, pin, GPIO_DIR_INPUT);
}

static void gpioAintcConf(void){

    /* Initialize the ARM interrupt control */
    IntAINTCInit();
 
    /* Registering gpioIsr */
    IntRegister(SYS_INT_GPIOINT1A, gpioIsr);
    //IntRegister(SYS_INT_GPIOINT1B, gpioIsr);
    
    /* Set the priority */
    IntPrioritySet(SYS_INT_GPIOINT1A, 0, AINTC_HOSTINT_ROUTE_IRQ);
    //IntPrioritySet(SYS_INT_GPIOINT1B, 1, AINTC_HOSTINT_ROUTE_IRQ);
    
    /* Enable the system interrupt */
    IntSystemEnable(SYS_INT_GPIOINT1A);
    //IntSystemEnable(SYS_INT_GPIOINT1B);
}

static void gpioIsr(void) {
    if(GPIOPinRead(getAddr(botoes[0].gpio_select), botoes[0].pino_saida)){
        ExecutarJogada(0);
    }else if(GPIOPinRead(getAddr(botoes[1].gpio_select), botoes[1].pino_saida)){
        ExecutarJogada(1);
    }else if(GPIOPinRead(getAddr(botoes[2].gpio_select), botoes[2].pino_saida)){
        ExecutarJogada(2);
    }else if(GPIOPinRead(getAddr(botoes[3].gpio_select), botoes[3].pino_saida)){
        ExecutarJogada(3);
    }else if(GPIOPinRead(getAddr(botoes[4].gpio_select), botoes[4].pino_saida)){
        ExecutarJogada(4);
    }else if(GPIOPinRead(getAddr(botoes[5].gpio_select), botoes[5].pino_saida)){
        ExecutarJogada(5);
    }else if(GPIOPinRead(getAddr(botoes[6].gpio_select), botoes[6].pino_saida)){
        ExecutarJogada(6);
    }else if(GPIOPinRead(getAddr(botoes[7].gpio_select), botoes[7].pino_saida)){
        ExecutarJogada(7);
    }else if(GPIOPinRead(getAddr(botoes[8].gpio_select), botoes[8].pino_saida)){
        ExecutarJogada(8);
    }
    Delay(1);
    /*	Clear wake interrupt	*/
	//HWREG(SOC_GPIO_1_REGS + 0x3C) = 0x1000;
	//HWREG(SOC_GPIO_1_REGS + 0x34) = 0x1000;
	HWREG(SOC_GPIO_1_REGS + 0x2C) = 0xFFFFFFFF;
}

static int gpioPinIntConf(unsigned int baseAdd, unsigned int intLine, unsigned int pinNumber){
    gpioPinIntEnable(baseAdd, intLine, pinNumber);
    return(0);
}

static void gpioPinIntEnable(unsigned int baseAdd,
                      unsigned int intLine,
                      unsigned int pinNumber){
    if(GPIO_INTC_LINE_1 == intLine){
        HWREG(baseAdd + GPIO_IRQSTATUS_SET(0)) = (1 << pinNumber);
    }else{
        HWREG(baseAdd + GPIO_IRQSTATUS_SET(1)) = (1 << pinNumber);
    }
}

static void gpioIntTypeSet(unsigned int baseAdd, unsigned int pinNumber, unsigned int eventType){
    eventType &= 0xFF;

    switch(eventType){
        case GPIO_INT_TYPE_NO_LEVEL:

            /* Disabling logic LOW level detect interrupt generation. */
            HWREG(baseAdd + GPIO_LEVELDETECT(0)) &= ~(1 << pinNumber);

            /* Disabling logic HIGH level detect interrupt generation. */
            HWREG(baseAdd + GPIO_LEVELDETECT(1)) &= ~(1 << pinNumber);

        break;

        case GPIO_INT_TYPE_LEVEL_LOW:

            /* Enabling logic LOW level detect interrupt geenration. */
            HWREG(baseAdd + GPIO_LEVELDETECT(0)) |= (1 << pinNumber);

            /* Disabling logic HIGH level detect interrupt generation. */
            HWREG(baseAdd + GPIO_LEVELDETECT(1)) &= ~(1 << pinNumber);

            /* Disabling rising edge detect interrupt generation. */
            HWREG(baseAdd + GPIO_RISINGDETECT) &= ~(1 << pinNumber);

            /* Disabling falling edge detect interrupt generation. */
            HWREG(baseAdd + GPIO_FALLINGDETECT) &= ~(1 << pinNumber);

        break;

        case GPIO_INT_TYPE_LEVEL_HIGH:

            /* Disabling logic LOW level detect interrupt generation. */
            HWREG(baseAdd + GPIO_LEVELDETECT(0)) &= ~(1 << pinNumber);

            /* Enabling logic HIGH level detect interrupt generation. */
            HWREG(baseAdd + GPIO_LEVELDETECT(1)) |= (1 << pinNumber);

            /* Disabling rising edge detect interrupt generation. */
            HWREG(baseAdd + GPIO_RISINGDETECT) &= ~(1 << pinNumber);

            /* Disabling falling edge detect interrupt generation. */
            HWREG(baseAdd + GPIO_FALLINGDETECT) &= ~(1 << pinNumber);
        
        break;

        case GPIO_INT_TYPE_BOTH_LEVEL:
            
            /* Enabling logic LOW level detect interrupt geenration. */
            HWREG(baseAdd + GPIO_LEVELDETECT(0)) |= (1 << pinNumber);

            /* Enabling logic HIGH level detect interrupt generation. */
            HWREG(baseAdd + GPIO_LEVELDETECT(1)) |= (1 << pinNumber);

            /* Disabling rising edge detect interrupt generation. */
            HWREG(baseAdd + GPIO_RISINGDETECT) &= ~(1 << pinNumber);

            /* Disabling falling edge detect interrupt generation. */
            HWREG(baseAdd + GPIO_FALLINGDETECT) &= ~(1 << pinNumber);
            
        break;

        case GPIO_INT_TYPE_NO_EDGE:
            
            /* Disabling rising edge detect interrupt generation. */
            HWREG(baseAdd + GPIO_RISINGDETECT) &= ~(1 << pinNumber);

            /* Disabling falling edge detect interrupt generation. */
            HWREG(baseAdd + GPIO_FALLINGDETECT) &= ~(1 << pinNumber);

        break;

        case GPIO_INT_TYPE_RISE_EDGE:

            /* Enabling rising edge detect interrupt generation. */
            HWREG(baseAdd + GPIO_RISINGDETECT) |= (1 << pinNumber);

            /* Disabling falling edge detect interrupt generation. */
            HWREG(baseAdd + GPIO_FALLINGDETECT) &= ~(1 << pinNumber);

            /* Disabling logic LOW level detect interrupt generation. */
            HWREG(baseAdd + GPIO_LEVELDETECT(0)) &= ~(1 << pinNumber);

            /* Disabling logic HIGH level detect interrupt generation. */
            HWREG(baseAdd + GPIO_LEVELDETECT(1)) &= ~(1 << pinNumber);

        break;

        case GPIO_INT_TYPE_FALL_EDGE:

            /* Disabling rising edge detect interrupt generation. */
            HWREG(baseAdd + GPIO_RISINGDETECT) &= ~(1 << pinNumber);

            /* Enabling falling edge detect interrupt generation. */
            HWREG(baseAdd + GPIO_FALLINGDETECT) |= (1 << pinNumber);

            /* Disabling logic LOW level detect interrupt generation. */
            HWREG(baseAdd + GPIO_LEVELDETECT(0)) &= ~(1 << pinNumber);

            /* Disabling logic HIGH level detect interrupt generation. */
            HWREG(baseAdd + GPIO_LEVELDETECT(1)) &= ~(1 << pinNumber);

        break;

        case GPIO_INT_TYPE_BOTH_EDGE:

            /* Enabling rising edge detect interrupt generation. */
            HWREG(baseAdd + GPIO_RISINGDETECT) |= (1 << pinNumber);

            /* Enabling falling edge detect interrupt generation. */
            HWREG(baseAdd + GPIO_FALLINGDETECT) |= (1 << pinNumber);

            /* Disabling logic LOW level detect interrupt generation. */
            HWREG(baseAdd + GPIO_LEVELDETECT(0)) &= ~(1 << pinNumber);

            /* Disabling logic HIGH level detect interrupt generation. */
            HWREG(baseAdd + GPIO_LEVELDETECT(1)) &= ~(1 << pinNumber);

        break;

        default:
        break;
    }
}

static void InicializaValoresLeds(){

    leds[0].LED_R = 23;
    leds[0].LED_G = 14;
    leds[0].LED_B = 12;
    leds[0].gpio_select = GPIO2;
    
    leds[1].LED_R = 10;
    leds[1].LED_G = 8;
    leds[1].LED_B = 6;
    leds[1].gpio_select = GPIO2;
    
    leds[2].LED_R = 16;
    leds[2].LED_G = 15;
    leds[2].LED_B = 13;
    leds[2].gpio_select = GPIO2;

    leds[3].LED_R = 11;
    leds[3].LED_G = 9;
    leds[3].LED_B = 7;
    leds[3].gpio_select = GPIO2;
    
    leds[4].LED_R = 10;
    leds[4].LED_G = 9;
    leds[4].LED_B = 8;
    leds[4].gpio_select = GPIO0;

    leds[5].LED_R = 30;
    leds[5].LED_G = 4;
    leds[5].LED_B = 0;
    leds[5].gpio_select = GPIO1;
    
    leds[6].LED_R = 19;
    leds[6].LED_G = 15;
    leds[6].LED_B = 14;
    leds[6].gpio_select = GPIO3;
    
    leds[7].LED_R = 17;
    leds[7].LED_G = 16;
    leds[7].LED_B = 21;
    leds[7].gpio_select = GPIO3;
    
    leds[8].LED_R = 3;
    leds[8].LED_G = 13;
    leds[8].LED_B = 5;
    leds[8].gpio_select = GPIO0;
    
}

static void InicializaValoresBotoes(){

    botoes[0].pino_saida = 1;
    botoes[0].gpio_select = GPIO1;

    botoes[1].pino_saida = 5;
    botoes[1].gpio_select = GPIO1;

    botoes[2].pino_saida = 29;
    botoes[2].gpio_select = GPIO1;

    botoes[3].pino_saida = 7;
    botoes[3].gpio_select = GPIO1;

    botoes[4].pino_saida = 14;
    botoes[4].gpio_select = GPIO1;

    botoes[5].pino_saida = 12;
    botoes[5].gpio_select = GPIO1;

    botoes[6].pino_saida = 28;
    botoes[6].gpio_select = GPIO1;

    botoes[7].pino_saida = 18;
    botoes[7].gpio_select = GPIO1;

    botoes[8].pino_saida = 19;
    botoes[8].gpio_select = GPIO1;

}

static void InicializaLeds(){
    for(int i = 0; i < LINHAS*COLUNAS; i++){
        Jogada[i / LINHAS][i % COLUNAS] = ESTADO_INICIAL;
        initLed(getAddr(leds[i].gpio_select), leds[i].gpio_select, leds[i].LED_R);
        initLed(getAddr(leds[i].gpio_select), leds[i].gpio_select, leds[i].LED_G);
        initLed(getAddr(leds[i].gpio_select), leds[i].gpio_select, leds[i].LED_B);
    }
}

static void InicializaBotoes(){
    for(int i = 0; i < LINHAS*COLUNAS; i++)
        initButton(getAddr(botoes[i].gpio_select), botoes[i].gpio_select, botoes[i].pino_saida);
}


static void ExecutarJogada(unsigned int pin){
    if(Jogada[pin / LINHAS][pin % COLUNAS] != ESTADO_INICIAL){
        JogadaInvalida();
        return;
    }
    if(jogador % 2 == 0){
        GPIOPinWrite(getAddr(leds[pin].gpio_select), leds[pin].LED_G, PIN_HIGH);
        Jogada[pin / LINHAS][pin % COLUNAS] = JOGADOR_1;
    }
    else{
        GPIOPinWrite(getAddr(leds[pin].gpio_select), leds[pin].LED_B, PIN_HIGH);
        Jogada[pin / LINHAS][pin % COLUNAS] = JOGADOR_2;
    }
    jogador++;
    jogada++;
    if(VerificarFimDeJogo(pin)){
        if(jogador % 2 == 0)
            pont_jogador2++;
        else
            pont_jogador1++;
        ConsoleUtilsPrintf("\n\n\r ##########################################\n\r");
        ConsoleUtilsPrintf("\r ############  ##  PLACAR   ## ############\n\r");
        ConsoleUtilsPrintf("\r ######## %s %d x %d %s #######\n\r",jogador1,pont_jogador1,pont_jogador2,jogador2);
        ConsoleUtilsPrintf("\r ##########################################\n\r");
        jogada = 0;
        return;
    }
    if(jogada % 9 == 0){
        ResetJogo();
        jogada = 0;
    }
}

static void JogadaInvalida(){
    for (int i = 0; i < LINHAS * COLUNAS; i++){
        if(Jogada[i / LINHAS][i % COLUNAS] == JOGADOR_1)
            GPIOPinWrite(getAddr(leds[i].gpio_select), leds[i].LED_G, PIN_LOW);
        else if(Jogada[i / LINHAS][i % COLUNAS] == JOGADOR_2)
            GPIOPinWrite(getAddr(leds[i].gpio_select), leds[i].LED_B, PIN_LOW);
    }
    for (int i = 0; i < LINHAS * COLUNAS; i++)
        GPIOPinWrite(getAddr(leds[i].gpio_select), leds[i].LED_R, PIN_HIGH);
    Delay(3);
    for (int i = 0; i < LINHAS * COLUNAS; i++)
        GPIOPinWrite(getAddr(leds[i].gpio_select), leds[i].LED_R, PIN_LOW);
    Recolorir();
}

static void Recolorir(){
    for (int i = 0; i < LINHAS * COLUNAS; i++){
        if(Jogada[i / LINHAS][i % COLUNAS] == JOGADOR_1)
            GPIOPinWrite(getAddr(leds[i].gpio_select), leds[i].LED_G, PIN_HIGH);
        else if(Jogada[i / LINHAS][i % COLUNAS] == JOGADOR_2)
            GPIOPinWrite(getAddr(leds[i].gpio_select), leds[i].LED_B, PIN_HIGH);      
    }
}

static int VerificarFimDeJogo(unsigned int pin){
    int linha = VerificarLinha(pin);
    if(linha >= LINHAS){
        ResetJogo();
        AcenderLinha(pin);
        return 1;
    }
    int coluna = VerificarColuna(pin);
    if(coluna >= COLUNAS){
        ResetJogo();
        AcenderColuna(pin);
        return 1;
    }
    
    int diagonaPrin = VerificarDiagonalPrincipal();
    if(diagonaPrin >= COLUNAS){  /* diagonaPrin < LINHAS  tambem seria valido*/
        ResetJogo();
        AcenderDiagonalPrincipal();
        return 1;
    }
    int diagonaSec = VerificarDiagonalSecundaria();
    if(diagonaSec >= COLUNAS){
        ResetJogo();
        AcenderDiagonalSecundaria();
        return 1;
    }
    return 0;
}

static int VerificarLinha(unsigned int pin){
    int i = 1;
    int linha = pin / LINHAS;
    int valor = Jogada[linha][0];
    if(valor == ESTADO_INICIAL)
        return 0;
    for (i = 1; i < COLUNAS; i++)
        if(Jogada[linha][i] != valor)
            break;
    return i;
}

static int VerificarColuna(unsigned int pin){
    int i = 1;
    int coluna = pin % COLUNAS;
    int valor = Jogada[0][coluna];
    if(valor == ESTADO_INICIAL)
        return 0;
    for (i = 1; i < LINHAS; i++)
        if(Jogada[i][coluna] != valor)
            break;
    return i;
}

static int VerificarDiagonalPrincipal(){
    int i = 1;
    int valor = Jogada[0][0];
    if(valor == ESTADO_INICIAL)
        return 0;
    for (i = 1; i < COLUNAS; i++)
        if(Jogada[i][i] != valor)
            break;
    return i;
}

static int VerificarDiagonalSecundaria(){
    int i = 1;
    int valor = Jogada[0][2];
    if(valor == ESTADO_INICIAL)
        return 0;
    for (i = 1; i < COLUNAS; i++)
        if(Jogada[i][2 - i] != valor)
            break;
    return i;
}

static void ResetJogo(){
    for (int i = 0; i < LINHAS * COLUNAS; i++){
        if(Jogada[i / LINHAS][i % COLUNAS] == JOGADOR_1)
            GPIOPinWrite(getAddr(leds[i].gpio_select), leds[i].LED_G, PIN_LOW);
        else if(Jogada[i / LINHAS][i % COLUNAS] == JOGADOR_2)
            GPIOPinWrite(getAddr(leds[i].gpio_select), leds[i].LED_B, PIN_LOW);
        Jogada[i / LINHAS][i % COLUNAS] = ESTADO_INICIAL;
    }
}

static void AcenderLinha(unsigned int pin){
    int linha = pin / LINHAS;
    linha *= LINHAS;
    for (int j = 0; j < 3; j++){
        for (int i = 0; i < LINHAS; i++)
            GPIOPinWrite(getAddr(leds[linha + i].gpio_select), leds[linha + i].LED_R, PIN_HIGH);
        Delay(3);
        for (int i = 0; i < LINHAS; i++)
            GPIOPinWrite(getAddr(leds[linha + i].gpio_select), leds[linha + i].LED_R, PIN_LOW);
        Delay(1);
    }
}

static void AcenderColuna(unsigned int pin){
    int coluna = pin % COLUNAS;
    for (int j = 0; j < 3; j++){
        for (int i = 0; i < COLUNAS; i++)
            GPIOPinWrite(getAddr(leds[coluna + LINHAS * i].gpio_select), leds[coluna + LINHAS * i].LED_R, PIN_HIGH);
        Delay(3);
        for (int i = 0; i < LINHAS; i++)
            GPIOPinWrite(getAddr(leds[coluna + LINHAS * i].gpio_select), leds[coluna + LINHAS * i].LED_R, PIN_LOW);
        Delay(1);
    }
}

static void AcenderDiagonalPrincipal(){
    for (int i = 0; i < 3; i++){
        GPIOPinWrite(getAddr(leds[0].gpio_select), leds[0].LED_R, PIN_HIGH);
        GPIOPinWrite(getAddr(leds[4].gpio_select), leds[4].LED_R, PIN_HIGH);
        GPIOPinWrite(getAddr(leds[8].gpio_select), leds[8].LED_R, PIN_HIGH);
        Delay(3);
        GPIOPinWrite(getAddr(leds[0].gpio_select), leds[0].LED_R, PIN_LOW);
        GPIOPinWrite(getAddr(leds[4].gpio_select), leds[4].LED_R, PIN_LOW);
        GPIOPinWrite(getAddr(leds[8].gpio_select), leds[8].LED_R, PIN_LOW);
        Delay(1);
    }
}

static void AcenderDiagonalSecundaria(){
    for (int i = 0; i < 3; i++){
        GPIOPinWrite(getAddr(leds[2].gpio_select), leds[2].LED_R, PIN_HIGH);
        GPIOPinWrite(getAddr(leds[4].gpio_select), leds[4].LED_R, PIN_HIGH);
        GPIOPinWrite(getAddr(leds[6].gpio_select), leds[6].LED_R, PIN_HIGH);
        Delay(3);
        GPIOPinWrite(getAddr(leds[2].gpio_select), leds[2].LED_R, PIN_LOW);
        GPIOPinWrite(getAddr(leds[4].gpio_select), leds[4].LED_R, PIN_LOW);
        GPIOPinWrite(getAddr(leds[6].gpio_select), leds[6].LED_R, PIN_LOW);
        Delay(1);
    }
}

static void ConfigurarBotoesInterrupcao(){
    for (int i = 0; i < LINHAS * COLUNAS; ++i){
        gpioPinIntConf(getAddr(botoes[i].gpio_select), GPIO_INTC_LINE_1, botoes[i].pino_saida);
        gpioIntTypeSet(getAddr(botoes[i].gpio_select), botoes[i].pino_saida, GPIO_INTC_TYPE_RISE_EDGE);
    }   
}

static void DMTimerSetUp(void){
    DMTimerReset(SOC_DMTIMER_2_REGS);
    DMTimerModeConfigure(SOC_DMTIMER_2_REGS, DMTIMER_AUTORLD_NOCMP_ENABLE);
}

/******************************* End of file *********************************/
