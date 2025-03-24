#include "x86.h"
#include "device.h"

extern int displayRow;
extern int displayCol;

extern uint32_t keyBuffer[MAX_KEYBUFFER_SIZE];
extern int bufferHead;
extern int bufferTail;

int tail=0;

void GProtectFaultHandle(struct TrapFrame *tf);

void KeyboardHandle(struct TrapFrame *tf);

void timerHandler(struct TrapFrame *tf);
void syscallHandle(struct TrapFrame *tf);
void sysWrite(struct TrapFrame *tf);
void sysPrint(struct TrapFrame *tf);
void sysRead(struct TrapFrame *tf);
void sysGetChar(struct TrapFrame *tf);
void sysGetStr(struct TrapFrame *tf);
void sysSetTimeFlag(struct TrapFrame *tf);
void sysGetTimeFlag(struct TrapFrame *tf);


void irqHandle(struct TrapFrame *tf) { // pointer tf = esp
	/*
	 * 中断处理程序
	 */
	/* Reassign segment register */
	asm volatile("movw %%ax, %%ds"::"a"(KSEL(SEG_KDATA)));

	switch(tf->irq) {
		// TODO1: 填好中断处理程序的调用
		case -1: break;
		case 0xd: GProtectFaultHandle(tf); break;
		case 0x80: syscallHandle(tf); break;
		default:assert(0);
	}
}

void GProtectFaultHandle(struct TrapFrame *tf){
	assert(0);
	return;
}

void KeyboardHandle(struct TrapFrame *tf){
	uint32_t code = getKeyCode();

	if(code == 0xe){ // 退格符
		//要求只能退格用户键盘输入的字符串，且最多退到当行行首
		if(displayCol>0&&displayCol>tail){
			displayCol--;
			uint16_t data = 0 | (0x0c << 8);
			int pos = (80*displayRow+displayCol)*2;
			asm volatile("movw %0, (%1)"::"r"(data),"r"(pos+0xb8000));
		}
	}else if(code == 0x1c){ // 回车符
		//处理回车情况
		keyBuffer[bufferTail++]='\n';
		displayRow++;
		displayCol=0;
		tail=0;
		if(displayRow==25){
			scrollScreen();
			displayRow=24;
			displayCol=0;
		}
	}else if(code < 0x81){ 
		// TODO？: 处理正常的字符
        char character = getChar(code);
        keyBuffer[bufferTail++] = character;
        uint16_t data = character | (0x0c << 8);
        int pos = (80 * displayRow + displayCol) * 2;
        asm volatile("movw %0, (%1)"::"r"(data), "r"(pos + 0xb8000));
        displayCol++;
        if (displayCol == 80) {
            displayRow++;
            displayCol = 0;
        }
        if (displayRow == 25) {
            scrollScreen();
            displayRow = 24;
            displayCol = 0;
        }		
	}
	updateCursor(displayRow, displayCol);

}

void timerHandler(struct TrapFrame *tf) {
	// TODO
	
}

void syscallHandle(struct TrapFrame *tf) {
	switch(tf->eax) { // syscall number
		case 0:
			sysWrite(tf);
			break; // for SYS_WRITE
		case 1:
			sysRead(tf);
			break; // for SYS_READ
		default:break;
	}
}

void sysWrite(struct TrapFrame *tf) {
	switch(tf->ecx) { // file descriptor
		case 0:
			sysPrint(tf);
			break; // for STD_OUT
		default:break;
	}
}

void sysPrint(struct TrapFrame *tf) {
	int sel =  USEL(SEG_UDATA);
	char *str = (char*)tf->edx;
	int size = tf->ebx;
	int i = 0;
	int pos = 0;
	char character = 0;
	uint16_t data = 0;
	asm volatile("movw %0, %%es"::"m"(sel));
	for (i = 0; i < size; i++) {
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str+i));
		// TODO1: 完成光标的维护和打印到显存
		if (character == '\n') {
			displayRow++;
			displayCol = 0;
			if (displayRow == 25) {
				displayRow = 24;
				displayCol = 0;
				scrollScreen();
			}
		}
		else {
			data = character | (0x0c << 8);
			pos = (80 * displayRow + displayCol) * 2;
			asm volatile("movw %0, (%1)"::"r"(data), "r"(pos + 0xb8000));
			displayCol++;
			if (displayCol == 80) {
				displayRow++;
				displayCol = 0;
				if (displayRow == 25) {
					displayRow = 24;
					displayCol = 0;
					scrollScreen();
				}
			}
		}
	}
	tail=displayCol;
	updateCursor(displayRow, displayCol);
}

void sysRead(struct TrapFrame *tf){
	switch(tf->ecx){ //file descriptor
		case 0:
			sysGetChar(tf);
			break; // for STD_IN
		case 1:
			sysGetStr(tf);
			break; // for STD_STR
		default:break;
	}
}

void sysGetChar(struct TrapFrame *tf){
	// TODO？: 自由实现
   // 等待键盘输入
   while (bufferHead == bufferTail) {
		enableInterrupt();
		while (bufferHead == bufferTail); // 等待新的按键输入
		disableInterrupt();
	}

	// 从键盘缓冲区读取一个字符
	tf->eax = keyBuffer[bufferHead];  // 返回值存入eax
	bufferHead = (bufferHead + 1) % MAX_KEYBUFFER_SIZE; // 更新缓冲区头指针
}

void sysGetStr(struct TrapFrame *tf){
	// TODO？: 自由实现
    int sel = USEL(SEG_UDATA); 
    char *str = (char*)tf->edx;    // 用户传入的缓冲区地址
    int size = tf->ebx;            // 最大读取长度
    int count = 0;                 // 已读取的字符数

    asm volatile("movw %0, %%es"::"m"(sel));

    while (count < size - 1) {  // 预留一位给'\0'
        // 等待键盘输入
        while (bufferHead == bufferTail) {
            enableInterrupt();
            while (bufferHead == bufferTail);
            disableInterrupt();
        }

        // 读取字符
        char ch = keyBuffer[bufferHead];
        bufferHead = (bufferHead + 1) % MAX_KEYBUFFER_SIZE;

        // 如果是回车，结束输入
        if (ch == '\n') {
            asm volatile("movb %0, %%es:(%1)"::"r"(ch),"r"(str + count));
            count++;
            break;
        }

        // 将字符写入用户空间缓冲区
        asm volatile("movb %0, %%es:(%1)"::"r"(ch),"r"(str + count));
        count++;
    }

    // 添加字符串结束符
    if (count < size) {
        asm volatile("movb %0, %%es:(%1)"::"r"('\0'),"r"(str + count));
    }

    tf->eax = count;  // 返回读取的字符数
}

void sysGetTimeFlag(struct TrapFrame *tf) {
	// TODO: 自由实现

}

void sysSetTimeFlag(struct TrapFrame *tf) {
	// TODO: 自由实现

}
