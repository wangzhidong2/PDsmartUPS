/*

  路由器应急补电系统程序（适配酷态科10号Plus + Wemos D1）

  核心功能：检测路由器12V供电电压，低于阈值时高电平触发继电器，接通酷态科应急供电

  硬件适配：A0口已接10K+2K分压电阻（正常分压2V），继电器接D3（高电平吸合）

  适配场景：对断电敏感的12V路由器，快速响应且防误触发

*/



// ====================== 硬件引脚定义 ======================

const int AO_DETECT = A0;       // 电压检测引脚（接分压电路，监测路由器12V供电）

const int RELAY_PIN = D6;       // 继电器控制引脚（高电平吸合，接通酷态科供电）



// ====================== 核心参数配置 ======================

const int VOLT_THRESHOLD = 400; // 电压触发阈值（A0正常2V对应模拟值≈620，400对应≈1.3V）

const int CHECK_INTERVAL = 200; // 主检测间隔（200ms，兼顾响应速度和CPU占用）

const int STABLE_CHECK = 2;     // 防抖检测次数（2次，减少误触发）

const int STABLE_INTERVAL = 50; // 防抖单次检测间隔（50ms，快速确认电压状态）



// ====================== 初始化函数 ======================

void setup() {

  // 初始化串口通信（波特率9600，用于调试查看电压值和切换状态）

  Serial.begin(9600);

  // 等待串口初始化完成（避免开机前几秒串口无输出）

  delay(1000);

  

  // 配置引脚模式：A0为输入（读电压），D3为输出（控继电器）

  pinMode(AO_DETECT, INPUT);

  pinMode(RELAY_PIN, OUTPUT);

  

  // 初始状态：继电器断开（低电平），避免开机误触发

  digitalWrite(RELAY_PIN, LOW);

  

  // 串口打印初始化完成提示（方便调试）

  Serial.println("===== 路由器应急补电系统初始化完成 =====");

  Serial.print("电压触发阈值（模拟值）：");

  Serial.println(VOLT_THRESHOLD);

  Serial.println("----------------------------------------");

}



// ====================== 主循环函数（核心逻辑） ======================

void loop() {

  // 定义变量：记录连续检测到电压低于阈值的次数

  int lowVoltCount = 0;



  // --------------------- 第一步：多次防抖检测电压 ---------------------

  // 连续检测2次，每次间隔50ms，避免电压波动误判

  for (int i = 0; i < STABLE_CHECK; i++) {

    // 读取A0口模拟值（范围0-1023，对应0-3.3V）

    int analogValue = analogRead(AO_DETECT);

    

    // 串口打印实时检测值（方便校准阈值和调试）

    Serial.print("第");

    Serial.print(i + 1);

    Serial.print("次检测 - A0模拟值:");

    Serial.println(analogValue);

    

    // 若当前值低于阈值，计数+1

    if (analogValue < VOLT_THRESHOLD) {

      lowVoltCount++;

    }

    

    // 单次检测间隔（防抖核心，避免瞬间波动）

    delay(STABLE_INTERVAL);

  }



  // --------------------- 第二步：根据检测结果控制继电器 ---------------------

  // 连续2次检测到电压低于阈值 → 判定为市电断电，触发继电器

  if (lowVoltCount >= STABLE_CHECK) {

    digitalWrite(RELAY_PIN, HIGH); // 高电平吸合继电器，接通酷态科供电

    Serial.println(">>> 市电断电已接通应急供电 <<<");

  } 

  // 电压正常 → 继电器断开，切回路由器原装电源

  else {

    digitalWrite(RELAY_PIN, LOW);  // 低电平断开继电器，关闭酷态科供电

    Serial.println(">>> 市电正常！已断开应急供电，切回原装电源 <<<");

  }



  // --------------------- 第三步：主检测间隔（控制整体响应速度） ---------------------

  Serial.println("----------------------------------------"); // 分隔线，方便看日志
