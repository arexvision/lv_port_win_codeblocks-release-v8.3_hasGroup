void init_buhlmann_algorithm(void)
{
    // Buhlmann减压算法初始化
    rt_kprintf("Initializing Buhlmann algorithm...\n");

    buhlmann.setSeaLevelAtmosphericPressure(1000.0f);  // 1.0 bar = 1000 mbar
    buhlmann.setNitrogenRateInGas(0.79f);              // 空气中氮气 79%

    // 设置 GF Low/GF High（梯度因子）
    buhlmann.setGFLow(0.40f);   // 首停梯度因子 40%
    buhlmann.setGFHigh(0.85f);  // 出水梯度因子 85%

    // 初始化隔舱状态
    DiveResult *initialCompartments = buhlmann.initializeCompartments();
    buhlmann.startDive(initialCompartments, 0);

    rt_kprintf("Buhlmann algorithm initialized (GF: %d/%d)\n", (int)(buhlmann.getGFLow() * 100), (int)(buhlmann.getGFHigh() * 100));
}
