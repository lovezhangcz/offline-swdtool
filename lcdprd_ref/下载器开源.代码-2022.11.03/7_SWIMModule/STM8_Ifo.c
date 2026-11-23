/**
* @brief 里面含有STM8相关的参数！
* @Author 
* @Data 
* \Attention 
*/

#include "STM8_Ifo.h"


char *const  Mcu_Model[42]=
{
	"STM8S003F3",
	"STM8S003K3",
	"STM8S005C6",
	"STM8S005K6",
	"STM8S007C8",
	"STM8S103F2",
	"STM8S103F3",
	"STM8S103K3",
	"STM8S105C4",
	"STM8S105K4",
	"STM8S105S4",
	"STM8S105C6",
	"STM8S105K6",
	"STM8S207C6",
	"STM8S207K6",
	"STM8S207R6",
	"STM8S207S6",
	"STM8S208C6",
	"STM8S208R6",
	"STM8S208S6",
	"STM8S207C8",
	"STM8S207K8",
	"STM8S207M8",
	"STM8S207R8",
	"STM8S207S8",
	"STM8S208C8",
	"STM8S208M8",
	"STM8S208R8",
	"STM8S208S8",
	"STM8S208CB",
	"STM8S207MB",
	"STM8S207RB",
	"STM8S207SB",
	"STM8S208CB",
	"STM8S208MB",
	"STM8S208RB",
	"STM8S208SB",
	"STM8S903F3",
	"STM8S903K3",
	"STM8SPLNB1"
};

MCU_INFO *CurStm8;


MCU_INFO STM8S_xx[]=
{		
	{64  ,128, 0x4000,  8, 0x8000,0},    //STM8S003F3, - 64每页，128字节EEPROM，地址0x4000开始，8KB大小，0x8000开始！
};



/**
	* @brief  按照顺序依次为：AF/AL/L/S/TL-小，中，大，和最后一个：STM8L101x
	* @Author 
	* @Data   
	* \Attention 
	*/
/**
 * @brief L与AL系列芯片
 * @data 	● low density STM8L101x （x2 x3只有这两个型号）
					● low density STM8L051F3, STM8L151C3/K3/K2/G3/G2/F3/F2
					● medium density STM8L052C6, STM8L151C6/C4/K6/K4/G6/G4, and
					STM8L152C6/C4/K6/K4
					● medium density STM8AL31xx/STM8AL3Lxx
					● medium+ density STM8L151R6 and STM8L152R6
					● high density STM8L052R8, STM8L151x8, STM8L152x8, STM8L162x8
 * @b   当然，其它类型也就是，S与A(F)系列 
 * @Attention 
 */





// 手册中说：8A系列，但是没有说明是不是包含了AH和AF系列！暂时认定为这两个系列！
// 8A：没有容量，只有：中容量：8-32 （128 Block）大容量 32-128（128Block）

// 容量判断法(AF和AH系列不准！可能要单独检查容量大小。)：尾号（部分是倒数第二个）： 2(4K) 3(8K) 4(16K) 6(32K) 8(64K) B(128K)  // 最小4K，最大8K
// 根据发现：选项字节都是1Block大小。

// 这个StoreIfo，需要更具体的检查下。仔细的检查！
MCU_INFO STM8_StoreIfo[]=
{
	// BLOCK ,EEPROMSIZE,EEPROMADDR,FLASHSIZE,FLASHADDR,OPTADDR
	{    64,       0,      0x4000,      0,      0x8000, 0x4800},  // AF-L  // 这个是不应该存在的。
	{    128,      0,      0x4000,      0,      0x8000, 0x4800},  // AF-M
	{    128,      0,      0x4000,      0,      0x8000, 0x4800},  // AF-H
	
	// AL 和 L 系列的手册，表明其OPTION BYTE不需要用补码的方式保证。
	{    64,       0,      0x1000,      0,      0x8000, 0x4800},  // AL-L
	{    128,      0,      0x1000,      0,      0x8000, 0x4800},  // AL-M
	{    128,      0,      0x1000,      0,      0x8000, 0x4800},  // AL-M+
	{    128,      0,      0x1000,      0,      0x8000, 0x4800},  // AL-H
	
	{    64,       0,      0x1000,      0,      0x8000, 0x4800},  // L-L  // 小容量，64Block (L与AL系列小容量：4-8  中容量：16-32 大容量 64 （大容量好像没有到128KB）)
																																// 根据手册：只有L101x小容量的Block大小是64，其它都是128，这个最好能进行测试！
	{    128,      0,      0x1000,      0,      0x8000, 0x4800},  // L-M  // 128Block 
	{    128,      0,      0x1000,      0,      0x8000, 0x4800},  // L-M+ // 2*128Block
	{    128,      0,      0x1000,      0,      0x8000, 0x4800},  // L-H  // 2*128Block 
	
	
	{    64,       0,      0x9800,      0,      0x8000, 0x4800},  // L-101x  // 根据STVP，这个应该被排除在外了，因为没有EEPROM
	
	
	{    64,       0,      0x4000,      0,      0x8000, 0x4800},  // S-L  手册说的是：8KB 属于小容量
	{    128,      0,      0x4000,      0,      0x8000, 0x4800},  // S-M  16-32KB  
	{    128,      0,      0x4000,      0,      0x8000, 0x4800},  // S-H  32-128KB  -  不过M和H的容量的Block都是一样的。
	// TL 系列未知！暂时用S系列代替
	{    64,       0,      0x4000,      0,      0x8000, 0x4800},  // TL-L
	{    128,      0,      0x4000,      0,      0x8000, 0x4800},  // TL-M
	{    128,      0,      0x4000,      0,      0x8000, 0x4800},  // TL-H
	
};

