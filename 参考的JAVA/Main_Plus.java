package BuhlmannZHL16;

import javax.swing.*;
import javax.swing.event.ChangeEvent;
import javax.swing.event.ChangeListener;
import java.awt.*;
import java.awt.event.*;
import java.awt.image.BufferedImage;
import java.util.*;
import java.util.List;
// java -cp Main_Plus_Class BuhlmannZHL16.Main_Plus
/**
 * 整合版潜水电脑模拟器 - Buhlmann ZHL-16
 * 包含 Trimix，多气体支持，GUI界面，减压计划等
 */
public class Main_Plus {

    // 速度倍率，1-60倍
    private static int speedMultiplier = 1;

    public static void main(String[] args) {

        final DiveSettings settings = new DiveSettings();
        final ZHL16 zhl16 = new ZHL16(settings, 0);

        JFrame diveComputerSimulator = new JFrame("Dive Computer Simulator");

        JPanel pressureSensorPanel = new JPanel();
        JPanel displayPanel = new JPanel();
        JPanel mainPanel = new JPanel(new BorderLayout());

        final PressureSensor pressureSensor = new PressureSensor(pressureSensorPanel, settings);
        final DiveDisplay diveDisplay = new DiveDisplay(displayPanel);

        // 速度控制面板
        JPanel speedPanel = new JPanel();
        speedPanel.setLayout(new FlowLayout(FlowLayout.CENTER));
        JLabel speedLabel = new JLabel("速度倍率(1-60):");
        final JTextField speedInput = new JTextField("1", 3);
        JButton speedButton = new JButton("设置");
        final JLabel currentSpeedLabel = new JLabel("当前: 1x");

        speedPanel.add(speedLabel);
        speedPanel.add(speedInput);
        speedPanel.add(speedButton);
        speedPanel.add(currentSpeedLabel);

        // ========== 3 Gas Nitrox 模式 ==========
        speedPanel.add(Box.createHorizontalStrut(20));
        final JCheckBox multiGasCheckBox = new JCheckBox("3 Gas 模式");
        speedPanel.add(multiGasCheckBox);
        final JLabel ascentPPO2Label = new JLabel("上升 PPO2:");
        final JTextField ascentPPO2Field = new JTextField(String.valueOf(settings.getAscentPPO2()), 4);
        ascentPPO2Label.setVisible(false);
        ascentPPO2Field.setVisible(false);

        // ========== Extended (Trimix) 模式 ==========
        speedPanel.add(Box.createHorizontalStrut(10));
        final JCheckBox trimixCheckBox = new JCheckBox("Extended (Trimix)");
        trimixCheckBox.setForeground(new Color(0, 128, 255));  // 蓝色
        speedPanel.add(trimixCheckBox);

        // Trimix 参数输入（初始隐藏）
        final JLabel trimixO2Label = new JLabel("O2%:");
        final JTextField trimixO2Field = new JTextField("18", 3);
        final JLabel trimixHeLabel = new JLabel("He%:");
        final JTextField trimixHeField = new JTextField("45", 3);
        final JLabel trimixInfoLabel = new JLabel("");
        trimixInfoLabel.setForeground(new Color(0, 128, 255));

        trimixO2Label.setVisible(false);
        trimixO2Field.setVisible(false);
        trimixHeLabel.setVisible(false);
        trimixHeField.setVisible(false);
        trimixInfoLabel.setVisible(false);

        speedPanel.add(trimixO2Label);
        speedPanel.add(trimixO2Field);
        speedPanel.add(trimixHeLabel);
        speedPanel.add(trimixHeField);
        speedPanel.add(trimixInfoLabel);

        // 气体切换按钮 (初始隐藏)
        final JButton gas1Button = new JButton("Gas1: Air");
        final JButton gas2Button = new JButton("Gas2: --");
        final JButton gas3Button = new JButton("Gas3: --");
        gas1Button.setBackground(Color.GREEN);  // 当前活动气体
        gas2Button.setEnabled(false);
        gas3Button.setEnabled(false);

        speedPanel.add(gas1Button);
        speedPanel.add(gas2Button);
        speedPanel.add(gas3Button);

        // 气体配置按钮
        final JButton configGasButton = new JButton("配置气体");
        speedPanel.add(configGasButton);

        // 减压计划按钮
        final JButton decoPlanButton = new JButton("减压计划");
        speedPanel.add(decoPlanButton);

        // 更优气体提示标签
        final JLabel betterGasLabel = new JLabel("");
        betterGasLabel.setForeground(new Color(255, 165, 0));  // 橙色
        speedPanel.add(betterGasLabel);

        // 单气体模式下的气体选择 (3 Gas 模式关闭时显示)
        speedPanel.add(Box.createHorizontalStrut(10));
        final JLabel gasLabel = new JLabel("气体:");
        String[] gasOptions = {"Air (21%)", "EAN32 (32%)", "EAN36 (36%)", "EAN40 (40%)", "EAN50 (50%)", "O2 (100%)"};
        final JComboBox<String> gasComboBox = new JComboBox<String>(gasOptions);
        speedPanel.add(gasLabel);
        speedPanel.add(gasComboBox);

        // 初始状态：隐藏 3 Gas 按钮
        gas1Button.setVisible(false);
        gas2Button.setVisible(false);
        gas3Button.setVisible(false);
        configGasButton.setVisible(false);

        // 更新 Trimix 信息显示
        final Runnable updateTrimixInfo = new Runnable() {
            @Override
            public void run() {
                if (settings.isTrimixMode()) {
                    Gas gas = settings.getActiveGas();
                    if (gas.isTrimix()) {
                        String info = gas.getTrimixInfo(settings.getSurfacePressure(),
                                settings.getDepthPerBar(), settings.getMaxEnd());
                        trimixInfoLabel.setText(info);
                    }
                } else {
                    trimixInfoLabel.setText("");
                }
            }
        };

        // 更新气体按钮显示
        final Runnable updateGasButtons = new Runnable() {
            @Override
            public void run() {
                Gas[] gases = settings.getGases();
                int activeIndex = settings.getActiveGasIndex();

                // Gas 1
                if (gases[0].isEnabled()) {
                    int o2 = (int) Math.round(gases[0].getOxygenAmount() * 100);
                    int he = (int) Math.round(gases[0].getHeAmount() * 100);
                    double mod = settings.calculateMODForGas(gases[0]);
                    String prefix = (activeIndex == 0) ? "A1" : "1";
                    gas1Button.setText(prefix + ": " + o2 + "/" + String.format("%02d", he) + " " + Math.round(mod) + "m");
                    gas1Button.setEnabled(true);
                    gas1Button.setBackground(activeIndex == 0 ? Color.GREEN : null);
                    gas1Button.setForeground(Color.BLACK);
                } else {
                    gas1Button.setText("1: Off");
                    gas1Button.setEnabled(false);
                    gas1Button.setBackground(null);
                    gas1Button.setForeground(Color.MAGENTA);
                }

                // Gas 2
                if (gases[1].isEnabled()) {
                    int o2 = (int) Math.round(gases[1].getOxygenAmount() * 100);
                    int he = (int) Math.round(gases[1].getHeAmount() * 100);
                    double mod = settings.calculateMODForGas(gases[1]);
                    String prefix = (activeIndex == 1) ? "A2" : "2";
                    gas2Button.setText(prefix + ": " + o2 + "/" + String.format("%02d", he) + " " + Math.round(mod) + "m");
                    gas2Button.setEnabled(true);
                    gas2Button.setBackground(activeIndex == 1 ? Color.GREEN : null);
                    gas2Button.setForeground(Color.BLACK);
                } else {
                    gas2Button.setText("2: Off");
                    gas2Button.setEnabled(false);
                    gas2Button.setBackground(null);
                    gas2Button.setForeground(Color.MAGENTA);
                }

                // Gas 3
                if (gases[2].isEnabled()) {
                    int o2 = (int) Math.round(gases[2].getOxygenAmount() * 100);
                    int he = (int) Math.round(gases[2].getHeAmount() * 100);
                    double mod = settings.calculateMODForGas(gases[2]);
                    String prefix = (activeIndex == 2) ? "A3" : "3";
                    gas3Button.setText(prefix + ": " + o2 + "/" + String.format("%02d", he) + " " + Math.round(mod) + "m");
                    gas3Button.setEnabled(true);
                    gas3Button.setBackground(activeIndex == 2 ? Color.GREEN : null);
                    gas3Button.setForeground(Color.BLACK);
                } else {
                    gas3Button.setText("3: Off");
                    gas3Button.setEnabled(false);
                    gas3Button.setBackground(null);
                    gas3Button.setForeground(Color.MAGENTA);
                }
            }
        };

        // Extended (Trimix) 模式切换
        trimixCheckBox.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                boolean trimixMode = trimixCheckBox.isSelected();
                settings.setTrimixMode(trimixMode);

                // 切换 UI 显示
                trimixO2Label.setVisible(trimixMode);
                trimixO2Field.setVisible(trimixMode);
                trimixHeLabel.setVisible(trimixMode);
                trimixHeField.setVisible(trimixMode);
                trimixInfoLabel.setVisible(trimixMode);
                ascentPPO2Label.setVisible(trimixMode || multiGasCheckBox.isSelected());
                ascentPPO2Field.setVisible(trimixMode || multiGasCheckBox.isSelected());

                if (trimixMode) {
                    gasComboBox.setVisible(false);
                    gasLabel.setVisible(false);
                    try {
                        double o2 = Double.parseDouble(trimixO2Field.getText()) / 100.0;
                        double he = Double.parseDouble(trimixHeField.getText()) / 100.0;
                        settings.setTrimixO2(o2);
                        settings.setTrimixHe(he);
                    } catch (NumberFormatException ex) { }
                    updateTrimixInfo.run();
                } else {
                    settings.setMultiGasMode(multiGasCheckBox.isSelected());
                    gasComboBox.setVisible(true);
                    gasLabel.setVisible(true);
                    if (!multiGasCheckBox.isSelected()) {
                        ascentPPO2Label.setVisible(false);
                        ascentPPO2Field.setVisible(false);
                        settings.setGas(createGasFromSelection(gasComboBox.getSelectedIndex()));
                    } else {
                        updateGasButtons.run();
                    }
                }
                zhl16.updateSettings(settings);
            }
        });

        // Trimix O2/He 输入变化监听
        ActionListener trimixInputListener = new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                if (settings.isTrimixMode()) {
                    try {
                        double o2 = Double.parseDouble(trimixO2Field.getText()) / 100.0;
                        double he = Double.parseDouble(trimixHeField.getText()) / 100.0;
                        settings.setTrimixO2(o2);
                        settings.setTrimixHe(he);
                        zhl16.updateSettings(settings);
                        updateTrimixInfo.run();
                    } catch (NumberFormatException ex) { }
                }
            }
        };
        trimixO2Field.addActionListener(trimixInputListener);
        trimixHeField.addActionListener(trimixInputListener);
        trimixO2Field.addFocusListener(new FocusAdapter() {
            @Override
            public void focusLost(FocusEvent e) {
                trimixInputListener.actionPerformed(null);
            }
        });
        trimixHeField.addFocusListener(new FocusAdapter() {
            @Override
            public void focusLost(FocusEvent e) {
                trimixInputListener.actionPerformed(null);
            }
        });

        ActionListener ascentPPO2InputListener = new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                try {
                    double ascentPPO2 = Double.parseDouble(ascentPPO2Field.getText().trim());
                    if (ascentPPO2 >= 1.0 && ascentPPO2 <= 1.6) {
                        settings.setAscentPPO2(ascentPPO2);
                        zhl16.updateSettings(settings);
                        if (settings.isMultiGasMode()) updateGasButtons.run();
                    }
                } catch (NumberFormatException ex) { }
            }
        };
        ascentPPO2Field.addActionListener(ascentPPO2InputListener);
        ascentPPO2Field.addFocusListener(new FocusAdapter() {
            @Override
            public void focusLost(FocusEvent e) {
                ascentPPO2InputListener.actionPerformed(null);
            }
        });

        // 3 Gas 模式切换
        multiGasCheckBox.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                boolean multiGas = multiGasCheckBox.isSelected();
                settings.setMultiGasMode(multiGas);

                gas1Button.setVisible(multiGas);
                gas2Button.setVisible(multiGas);
                gas3Button.setVisible(multiGas);
                configGasButton.setVisible(multiGas);
                gasComboBox.setVisible(!multiGas);
                gasLabel.setVisible(!multiGas);
                ascentPPO2Label.setVisible(multiGas);
                ascentPPO2Field.setVisible(multiGas);

                if (multiGas) {
                    ascentPPO2InputListener.actionPerformed(null);
                    updateGasButtons.run();
                }
                zhl16.updateSettings(settings);
            }
        });

        // 气体切换按钮事件
        gas1Button.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                if (zhl16.switchGas(0)) {
                    updateGasButtons.run();
                    System.out.println("切换到 Gas 1: " + settings.getGases()[0]);
                } else {
                    JOptionPane.showMessageDialog(diveComputerSimulator,
                            "无法切换到 Gas 1，当前深度超过 MOD", "切换失败", JOptionPane.WARNING_MESSAGE);
                }
            }
        });

        gas2Button.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                if (zhl16.switchGas(1)) {
                    updateGasButtons.run();
                    System.out.println("切换到 Gas 2: " + settings.getGases()[1]);
                } else {
                    JOptionPane.showMessageDialog(diveComputerSimulator,
                            "无法切换到 Gas 2，当前深度超过 MOD", "切换失败", JOptionPane.WARNING_MESSAGE);
                }
            }
        });

        gas3Button.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                if (zhl16.switchGas(2)) {
                    updateGasButtons.run();
                    System.out.println("切换到 Gas 3: " + settings.getGases()[2]);
                } else {
                    JOptionPane.showMessageDialog(diveComputerSimulator,
                            "无法切换到 Gas 3，当前深度超过 MOD", "切换失败", JOptionPane.WARNING_MESSAGE);
                }
            }
        });

        // 气体配置对话框
        configGasButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                showGasConfigDialog(diveComputerSimulator, settings, updateGasButtons, zhl16);
            }
        });

        // 减压计划对话框
        decoPlanButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                showDecoPlanDialog(diveComputerSimulator, settings, zhl16, pressureSensor);
            }
        });

        gasComboBox.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                int index = gasComboBox.getSelectedIndex();
                Gas newGas = createGasFromSelection(index);
                settings.setTrimixMode(false);
                settings.setMultiGasMode(false);
                multiGasCheckBox.setSelected(false);
                ascentPPO2Label.setVisible(false);
                ascentPPO2Field.setVisible(false);
                settings.setGas(newGas);
                zhl16.updateSettings(settings);
            }
        });

        // GF 保守度设置
        speedPanel.add(Box.createHorizontalStrut(20));
        JLabel gfLabel = new JLabel("GF:");
        String[] gfOptions = {"Low (20/70)", "30/70", "Med (30/80)", "High (40/85)", "45/80", "Max (50/95)"};
        final JComboBox<String> gfComboBox = new JComboBox<String>(gfOptions);
        gfComboBox.setSelectedIndex(2);
        speedPanel.add(gfLabel);
        speedPanel.add(gfComboBox);

        // Last decompression stop selector (3m / 6m)
        speedPanel.add(Box.createHorizontalStrut(20));
        speedPanel.add(new JLabel("最后减压站:"));
        String[] lastStopOptions = {"3 米", "6 米"};
        final JComboBox<String> lastStopComboBox = new JComboBox<String>(lastStopOptions);
        lastStopComboBox.setSelectedIndex(settings.getLastStopDepth() == 6 ? 1 : 0);
        speedPanel.add(lastStopComboBox);

        // CEIL_L 显示标签
        speedPanel.add(Box.createHorizontalStrut(20));
        final JLabel ceilLabel = new JLabel("CEIL_L: --");
        ceilLabel.setForeground(Color.BLUE);
        speedPanel.add(ceilLabel);

        // GF 实时显示标签
        speedPanel.add(Box.createHorizontalStrut(10));
        final JLabel gfInfoLabel = new JLabel("GF@: --");
        gfInfoLabel.setForeground(new Color(128, 0, 128));
        speedPanel.add(gfInfoLabel);

        // GF Low Depth 显示
        final JLabel gfLowDepthLabel = new JLabel("1stStop: --");
        gfLowDepthLabel.setForeground(new Color(0, 128, 0));
        speedPanel.add(gfLowDepthLabel);

        // 调试按钮
        JButton debugButton = new JButton("调试GF");
        debugButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                zhl16.printDecoDebug();
            }
        });
        speedPanel.add(debugButton);

        // 减压测试按钮
        JButton decoTestButton = new JButton("减压测试");
        decoTestButton.setForeground(new Color(0, 128, 0));
        decoTestButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                showDecoTestDialog(diveComputerSimulator, settings);
            }
        });
        speedPanel.add(decoTestButton);

        // NDL 模拟按钮
        JButton ndlTestButton = new JButton("NDL模拟");
        ndlTestButton.setForeground(new Color(0, 100, 200));
        ndlTestButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                showNDLTestDialog(diveComputerSimulator, settings);
            }
        });
        speedPanel.add(ndlTestButton);

        gfComboBox.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                int index = gfComboBox.getSelectedIndex();
                GradientFactors newGf;
                switch (index) {
                    case 0: newGf = new GradientFactors(0.20, 0.70, settings.getSurfacePressure()); break;
                    case 1: newGf = new GradientFactors(0.30, 0.70, settings.getSurfacePressure()); break;
                    case 2: newGf = new GradientFactors(0.30, 0.80, settings.getSurfacePressure()); break;
                    case 3: newGf = new GradientFactors(0.40, 0.85, settings.getSurfacePressure()); break;
                    case 4: newGf = new GradientFactors(0.45, 0.80, settings.getSurfacePressure()); break;
                    case 5: newGf = new GradientFactors(0.50, 0.95, settings.getSurfacePressure()); break;
                    default: newGf = new GradientFactors(0.30, 0.80, settings.getSurfacePressure()); break;
                }
                settings.setGf(newGf);
                zhl16.updateSettings(settings);
            }
        });

        lastStopComboBox.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                int lastStop = lastStopComboBox.getSelectedIndex() == 1 ? 6 : 3;
                settings.setLastStopDepth(lastStop);
                zhl16.updateSettings(settings);
                System.out.println("最后减压站切换为: " + lastStop + "m");
            }
        });

        speedButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                try {
                    int speed = Integer.parseInt(speedInput.getText().trim());
                    if (speed < 1) speed = 1;
                    if (speed > 60) speed = 60;
                    speedMultiplier = speed;
                    speedInput.setText(String.valueOf(speed));
                    currentSpeedLabel.setText("当前: " + speed + "x");
                } catch (NumberFormatException ex) {
                    JOptionPane.showMessageDialog(diveComputerSimulator, "请输入1-60的整数", "输入错误", JOptionPane.ERROR_MESSAGE);
                }
            }
        });

        speedInput.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                speedButton.doClick();
            }
        });

        speedPanel.removeAll();
        speedPanel.setLayout(new FlowLayout(FlowLayout.LEFT));
        JButton settingsButton = new JButton("参数设置");
        speedPanel.add(settingsButton);
        speedPanel.add(decoPlanButton);
        speedPanel.add(currentSpeedLabel);
        speedPanel.add(ceilLabel);
        speedPanel.add(gfInfoLabel);
        speedPanel.add(gfLowDepthLabel);
        speedPanel.add(betterGasLabel);

        final JDialog settingsDialog = new JDialog(diveComputerSimulator, "参数设置", false);
        settingsDialog.setDefaultCloseOperation(JDialog.HIDE_ON_CLOSE);
        settingsDialog.setSize(720, 360);
        JPanel settingsContent = new JPanel();
        settingsContent.setLayout(new BoxLayout(settingsContent, BoxLayout.Y_AXIS));
        settingsContent.setBorder(BorderFactory.createEmptyBorder(10, 10, 10, 10));

        JPanel speedRow = new JPanel(new FlowLayout(FlowLayout.LEFT));
        speedRow.setBorder(BorderFactory.createTitledBorder("Simulation"));
        speedRow.add(speedLabel);
        speedRow.add(speedInput);
        speedRow.add(speedButton);
        settingsContent.add(speedRow);

        JPanel gasRow = new JPanel(new FlowLayout(FlowLayout.LEFT));
        gasRow.setBorder(BorderFactory.createTitledBorder("Gas"));
        gasRow.add(multiGasCheckBox);
        gasRow.add(ascentPPO2Label);
        gasRow.add(ascentPPO2Field);
        gasRow.add(trimixCheckBox);
        gasRow.add(gasLabel);
        gasRow.add(gasComboBox);
        settingsContent.add(gasRow);

        JPanel trimixRow = new JPanel(new FlowLayout(FlowLayout.LEFT));
        trimixRow.add(trimixO2Label);
        trimixRow.add(trimixO2Field);
        trimixRow.add(trimixHeLabel);
        trimixRow.add(trimixHeField);
        trimixRow.add(trimixInfoLabel);
        settingsContent.add(trimixRow);

        JPanel gasSwitchRow = new JPanel(new FlowLayout(FlowLayout.LEFT));
        gasSwitchRow.add(gas1Button);
        gasSwitchRow.add(gas2Button);
        gasSwitchRow.add(gas3Button);
        gasSwitchRow.add(configGasButton);
        settingsContent.add(gasSwitchRow);

        JPanel decoRow = new JPanel(new FlowLayout(FlowLayout.LEFT));
        decoRow.setBorder(BorderFactory.createTitledBorder("Decompression"));
        decoRow.add(gfLabel);
        decoRow.add(gfComboBox);
        decoRow.add(new JLabel("最后减压站:"));
        decoRow.add(lastStopComboBox);
        settingsContent.add(decoRow);

        JPanel environmentRow = new JPanel(new FlowLayout(FlowLayout.LEFT));
        environmentRow.setBorder(BorderFactory.createTitledBorder("Environment"));
        final JComboBox<DiveSettings.WaterDensity> waterDensityComboBox =
                new JComboBox<DiveSettings.WaterDensity>(DiveSettings.WaterDensity.values());
        waterDensityComboBox.setSelectedItem(settings.getWaterDensity());
        final JTextField altitudeField = new JTextField("0", 6);
        final JLabel surfacePressureLabel = new JLabel(String.format("Surface: %.3f bar", settings.getSurfacePressure()));
        JButton applyAltitudeButton = new JButton("Apply");
        environmentRow.add(new JLabel("Water:"));
        environmentRow.add(waterDensityComboBox);
        environmentRow.add(new JLabel("Altitude(m):"));
        environmentRow.add(altitudeField);
        environmentRow.add(applyAltitudeButton);
        environmentRow.add(surfacePressureLabel);
        settingsContent.add(environmentRow);

        JPanel toolsRow = new JPanel(new FlowLayout(FlowLayout.LEFT));
        toolsRow.setBorder(BorderFactory.createTitledBorder("Tools"));
        toolsRow.add(debugButton);
        toolsRow.add(ndlTestButton);
        settingsContent.add(toolsRow);

        settingsDialog.add(new JScrollPane(settingsContent), BorderLayout.CENTER);
        settingsButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                settingsDialog.setLocationRelativeTo(diveComputerSimulator);
                settingsDialog.setVisible(true);
            }
        });
        waterDensityComboBox.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                DiveSettings.WaterDensity density = (DiveSettings.WaterDensity) waterDensityComboBox.getSelectedItem();
                if (density != null) {
                    settings.setWaterDensity(density);
                    pressureSensor.updateEnvironment(settings);
                    zhl16.updateSettings(settings);
                }
            }
        });
        applyAltitudeButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                try {
                    double altitude = Double.parseDouble(altitudeField.getText().trim());
                    if (altitude < -500 || altitude > 6000) {
                        throw new NumberFormatException();
                    }
                    settings.setSurfacePressure(DiveSettings.altitudeToBar(altitude));
                    pressureSensor.updateEnvironment(settings);
                    zhl16.updateSettings(settings);
                    surfacePressureLabel.setText(String.format("Surface: %.3f bar", settings.getSurfacePressure()));
                } catch (NumberFormatException ex) {
                    JOptionPane.showMessageDialog(settingsDialog, "Altitude range: -500 to 6000 m",
                            "Input Error", JOptionPane.ERROR_MESSAGE);
                }
            }
        });
        altitudeField.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                applyAltitudeButton.doClick();
            }
        });

        diveComputerSimulator.setSize(new Dimension(1024, 768));
        diveComputerSimulator.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        diveComputerSimulator.setLocationRelativeTo(null);
        mainPanel.add(pressureSensorPanel, BorderLayout.EAST);
        mainPanel.add(displayPanel, BorderLayout.CENTER);
        mainPanel.add(speedPanel, BorderLayout.NORTH);
        diveComputerSimulator.add(mainPanel);
        diveComputerSimulator.setVisible(true);

        java.util.Timer seconds = new java.util.Timer();

        TimerTask timerTick = new TimerTask() {
            @Override
            public void run() {
                DiveDataPoint divePoint = zhl16.dive(pressureSensor.getPressure(), speedMultiplier);

                int courser = 0;
                diveDisplay.drawHorizontalLine(22, 2, diveDisplay.colors.LIGHTBLUE);
                diveDisplay.drawHorizontalLine(106, 2, diveDisplay.colors.YELLOWGREEN);

                Gas activeGas = settings.getActiveGas();
                int gasColor = diveDisplay.colors.LIGHTBLUE;

                if (settings.isMultiGasMode() && zhl16.hasBetterGasAvailable()) {
                    gasColor = diveDisplay.colors.ORANGEYELLOW;
                    betterGasLabel.setText("⚠ 有更优气体可用!");
                } else {
                    betterGasLabel.setText("");
                }

                courser = 5;
                courser += diveDisplay.drawString(courser,20, activeGas.getOxygenAmount()*100 + "% O", gasColor, diveDisplay.fontArialNarrow12pB, true);
                courser += diveDisplay.drawString(courser, 28, "² ", gasColor, diveDisplay.fontArialNarrow12pB,true);
                courser += diveDisplay.drawString(courser,20,"max:" + Math.round(zhl16.barToMeter(zhl16.getMaxAllowedDepth(),settings)-0.5) + "m", gasColor, diveDisplay.fontArialNarrow12pB,true);

                if (settings.isMultiGasMode()) {
                    int gasNum = settings.getActiveGasIndex() + 1;
                    diveDisplay.drawString(150, 20, "G" + gasNum, gasColor, diveDisplay.fontArialNarrow12pB, false);
                }

                double ceilingLow = zhl16.barToMeter(zhl16.getCurrentCeilingWithGFLow(), settings);
                ceilLabel.setText("CEIL_L: " + Math.round(ceilingLow*10)/10.0 + "m");

                double currentDepthMeter = zhl16.barToMeter(divePoint.getDepthInBar(), settings);
                double[] gfInfo = zhl16.getGFInfo(currentDepthMeter);

                boolean hasDecoObligation = divePoint.getNdl() <= 0 || divePoint.getStopTime() > 0;

                int gfLow = (int)(gfInfo[0] * 100);
                int gfHigh = (int)(gfInfo[1] * 100);
                gfInfoLabel.setText(String.format("GF: %d/%d", gfLow, gfHigh));

                if (hasDecoObligation && gfInfo[2] > 1) {
                    int currentGF = (int)(gfInfo[3] * 100);
                    gfLowDepthLabel.setText(String.format("GF@%.0fm: %d%%", currentDepthMeter, currentGF));
                } else {
                    gfLowDepthLabel.setText(String.format("GF: %d%%", gfHigh));
                }

                int totalSec = (int) Math.round(divePoint.getTime());
                int dispMin = totalSec / 60;
                int dispSec = totalSec % 60;
                String timeStr = String.format("%d:%02d", dispMin, dispSec);
                diveDisplay.drawString(50, 126, timeStr, diveDisplay.colors.YELLOWGREEN, diveDisplay.fontArialNarrow12pB, false);
                diveDisplay.drawString(70, 126, "" + Math.round(zhl16.barToMeter(divePoint.getMaxDepth(),settings)*10)/10 + "m", diveDisplay.colors.YELLOWGREEN, diveDisplay.fontArialNarrow12pB,true);
                diveDisplay.drawString(160, 126, 0 + "°C", diveDisplay.colors.YELLOWGREEN, diveDisplay.fontArialNarrow12pB, false);

                diveDisplay.drawString(2, 42, "Depth:", diveDisplay.colors.RED, diveDisplay.fontArialNarrow12pB, true);

                courser = 160;
                courser -= diveDisplay.drawString(courser, 53, "m", diveDisplay.colors.RED, diveDisplay.fontArialNarrow12pB, false);
                courser -= diveDisplay.drawString(courser, 50, Math.round(zhl16.barToMeter(divePoint.getDepthInBar(), settings)*10)/10d + "", diveDisplay.colors.RED, diveDisplay.fontTahoma22pB, false);

                if (divePoint.getNdl()>0){
                    diveDisplay.drawString(2, 74, "NDL:", diveDisplay.colors.GREEN, diveDisplay.fontArialNarrow12pB, true);
                    if (divePoint.getNdl()<5940) {
                        courser = 160;
                        courser -= diveDisplay.drawString(courser, 85, "min", diveDisplay.colors.GREEN, diveDisplay.fontArialNarrow12pB, false);
                        courser -= diveDisplay.drawString(courser, 82, Math.round(divePoint.getNdl() / 60) + "", diveDisplay.colors.GREEN, diveDisplay.fontTahoma22pB, false);

                        double depthM = zhl16.barToMeter(divePoint.getDepthInBar(), settings);
                        int gfHighVal = (int)(settings.getGf().getHigh() * 100);
                        System.out.printf("[%d:%02d] 深度:%.1fm | NDL:%dmin | 使用GF High:%d%% (未进入减压)%n",
                                (int)(divePoint.getTime() / 60), (int)(divePoint.getTime() % 60),
                                depthM, (int)(divePoint.getNdl() / 60), gfHighVal);
                    }else{
                        diveDisplay.drawString(140, 82, "- -", diveDisplay.colors.GREEN, diveDisplay.fontTahoma22pB, false);
                    }
                } else{
                    diveDisplay.drawString(2, 74, "Stop:", diveDisplay.colors.PURPLE, diveDisplay.fontArialNarrow12pB, true);
                    diveDisplay.drawString(2, 101, "TTS:" + formatDuration(divePoint.getTts()), diveDisplay.colors.PURPLE, diveDisplay.fontArialNarrow12pB, true);

                    courser = 160;
                    courser -= diveDisplay.drawString(courser, 85, "m", diveDisplay.colors.PURPLE, diveDisplay.fontArialNarrow12pB, false);
                    courser -= diveDisplay.drawString(courser, 82, Math.round(zhl16.barToMeter(divePoint.getStopDepth(), settings)) + "", diveDisplay.colors.PURPLE, diveDisplay.fontTahoma22pB, false);
                    courser -= diveDisplay.drawString(courser, 82, Math.round((divePoint.getStopTime()/60)+0.5) + "-", diveDisplay.colors.PURPLE, diveDisplay.fontTahoma22pB, false);

                    int stopMin = (int)(divePoint.getStopTime() / 60);
                    int stopSec = (int)(divePoint.getStopTime() % 60);
                    double stopDepthMeter = zhl16.barToMeter(divePoint.getStopDepth(), settings);
                    double gfAtStop = zhl16.getCurrentGF(stopDepthMeter);
                    double gfLowDepth = zhl16.getGFLowDepthMeter();
                    System.out.printf("[%d:%02d] 深度:%.1fm | DECO站:%dm 时间:%d:%02d (%.1f秒) TTS:%.0f秒 | GF@站:%.0f%% 首停深度:%.0fm%n",
                            (int)(divePoint.getTime() / 60), (int)(divePoint.getTime() % 60),
                            zhl16.barToMeter(divePoint.getDepthInBar(), settings),
                            (int)Math.round(stopDepthMeter),
                            stopMin, stopSec, divePoint.getStopTime(),
                            divePoint.getTts(),
                            gfAtStop * 100, gfLowDepth);
                    System.out.printf("TTS:%s%n", formatDuration(divePoint.getTts()));
                }

                diveDisplay.antiAliasing();
                diveDisplay.updateDisplay();
                diveDisplay.clearBuffer();
            }
        };

        seconds.schedule(timerTick, 0, 1000);
    }

    static String formatDuration(double seconds) {
        int totalSeconds = Math.max(0, (int) Math.round(seconds));
        return String.format("%d:%02d", totalSeconds / 60, totalSeconds % 60);
    }

    static Gas createGasFromSelection(int index) {
        switch (index) {
            case 1: return new Gas(0.32);
            case 2: return new Gas(0.36);
            case 3: return new Gas(0.40);
            case 4: return new Gas(0.50);
            case 5: return new Gas(1.00);
            case 0:
            default: return new Gas(0.21);
        }
    }

    private static void showGasConfigDialog(JFrame parent, final DiveSettings settings,
                                            final Runnable updateCallback, final ZHL16 zhl16) {
        String title = settings.isTrimixMode() ? "Trimix Gases" : "Nitrox Gases";
        JDialog dialog = new JDialog(parent, title, true);

        int rows = settings.isTrimixMode() ? 6 : 5;
        dialog.setLayout(new BorderLayout());
        dialog.setSize(settings.isTrimixMode() ? 540 : 440,
                settings.isTrimixMode() ? 430 : 350);
        dialog.setLocationRelativeTo(parent);

        JPanel mainPanel = new JPanel(new GridLayout(rows, 1, 5, 5));
        mainPanel.setBorder(BorderFactory.createEmptyBorder(10, 10, 10, 10));

        JPanel headerPanel;
        if (settings.isTrimixMode()) {
            headerPanel = new JPanel(new GridLayout(1, 6));
            headerPanel.add(new JLabel("#", SwingConstants.CENTER));
            headerPanel.add(new JLabel("On", SwingConstants.CENTER));
            headerPanel.add(new JLabel("O2%", SwingConstants.CENTER));
            headerPanel.add(new JLabel("He%", SwingConstants.CENTER));
            headerPanel.add(new JLabel("MOD", SwingConstants.CENTER));
            headerPanel.add(new JLabel("MND", SwingConstants.CENTER));
        } else {
            headerPanel = new JPanel(new GridLayout(1, 4));
            headerPanel.add(new JLabel("#", SwingConstants.CENTER));
            headerPanel.add(new JLabel("On", SwingConstants.CENTER));
            headerPanel.add(new JLabel("O2%", SwingConstants.CENTER));
            headerPanel.add(new JLabel("MOD", SwingConstants.CENTER));
        }
        mainPanel.add(headerPanel);

        Gas[] gases = settings.getGases();
        settings.applyPPO2LimitsToGases();

        final JCheckBox[] enabledBoxes = new JCheckBox[3];
        final JTextField[] o2Fields = new JTextField[3];
        final JTextField[] heFields = new JTextField[3];
        final JLabel[] modLabels = new JLabel[3];
        final JLabel[] mndLabels = new JLabel[3];
        final JLabel[] numLabels = new JLabel[3];

        for (int i = 0; i < 3; i++) {
            final int gasIndex = i;
            JPanel gasPanel;

            if (settings.isTrimixMode()) {
                gasPanel = new JPanel(new GridLayout(1, 6));
            } else {
                gasPanel = new JPanel(new GridLayout(1, 4));
            }

            String numText = (i == settings.getActiveGasIndex()) ? "A" + (i+1) : String.valueOf(i+1);
            numLabels[i] = new JLabel(numText, SwingConstants.CENTER);
            if (i == settings.getActiveGasIndex()) {
                numLabels[i].setForeground(Color.BLUE);
            }
            gasPanel.add(numLabels[i]);

            enabledBoxes[i] = new JCheckBox(gases[i].isEnabled() ? "On" : "Off", gases[i].isEnabled());
            enabledBoxes[i].setHorizontalAlignment(SwingConstants.CENTER);
            if (!gases[i].isEnabled()) {
                enabledBoxes[i].setForeground(Color.MAGENTA);
            }
            gasPanel.add(enabledBoxes[i]);

            int o2Display = gases[i].isEnabled() ? (int)(gases[i].getOxygenAmount() * 100) : 0;
            o2Fields[i] = new JTextField(String.valueOf(o2Display), 4);
            o2Fields[i].setHorizontalAlignment(JTextField.CENTER);
            gasPanel.add(o2Fields[i]);

            if (settings.isTrimixMode()) {
                int heDisplay = gases[i].isEnabled() ? (int)(gases[i].getHeAmount() * 100) : 0;
                heFields[i] = new JTextField(String.valueOf(heDisplay), 4);
                heFields[i].setHorizontalAlignment(JTextField.CENTER);
                gasPanel.add(heFields[i]);
            }

            String modText = gases[i].isEnabled() ?
                    Math.round(settings.calculateMODForGas(gases[i])) + "m" : "- m";
            modLabels[i] = new JLabel(modText, SwingConstants.CENTER);
            if (!gases[i].isEnabled()) {
                modLabels[i].setForeground(Color.MAGENTA);
            }
            gasPanel.add(modLabels[i]);

            if (settings.isTrimixMode()) {
                String mndText = gases[i].isEnabled() ?
                        (int)gases[i].calculateMND(settings.getMaxEnd()) + "m" : "- m";
                mndLabels[i] = new JLabel(mndText, SwingConstants.CENTER);
                if (!gases[i].isEnabled()) {
                    mndLabels[i].setForeground(Color.MAGENTA);
                }
                gasPanel.add(mndLabels[i]);
            }

            ActionListener updateListener = new ActionListener() {
                @Override
                public void actionPerformed(ActionEvent e) {
                    updateMODMND(gasIndex, o2Fields, heFields, modLabels, mndLabels, enabledBoxes, settings);
                }
            };
            o2Fields[i].addActionListener(updateListener);
            if (settings.isTrimixMode() && heFields[i] != null) {
                heFields[i].addActionListener(updateListener);
            }
            o2Fields[i].addFocusListener(new FocusAdapter() {
                @Override
                public void focusLost(FocusEvent e) {
                    updateMODMND(gasIndex, o2Fields, heFields, modLabels, mndLabels, enabledBoxes, settings);
                }
            });
            if (settings.isTrimixMode() && heFields[i] != null) {
                heFields[i].addFocusListener(new FocusAdapter() {
                    @Override
                    public void focusLost(FocusEvent e) {
                        updateMODMND(gasIndex, o2Fields, heFields, modLabels, mndLabels, enabledBoxes, settings);
                    }
                });
            }

            enabledBoxes[i].addActionListener(new ActionListener() {
                @Override
                public void actionPerformed(ActionEvent e) {
                    boolean enabled = enabledBoxes[gasIndex].isSelected();
                    enabledBoxes[gasIndex].setText(enabled ? "On" : "Off");
                    enabledBoxes[gasIndex].setForeground(enabled ? Color.BLACK : Color.MAGENTA);
                    modLabels[gasIndex].setForeground(enabled ? Color.BLACK : Color.MAGENTA);
                    if (settings.isTrimixMode() && mndLabels[gasIndex] != null) {
                        mndLabels[gasIndex].setForeground(enabled ? Color.BLACK : Color.MAGENTA);
                    }
                    if (!enabled) {
                        o2Fields[gasIndex].setText("00");
                        modLabels[gasIndex].setText("- m");
                        if (settings.isTrimixMode() && heFields[gasIndex] != null) {
                            heFields[gasIndex].setText("00");
                            mndLabels[gasIndex].setText("- m");
                        }
                    } else {
                        o2Fields[gasIndex].setText("21");
                        if (settings.isTrimixMode() && heFields[gasIndex] != null) {
                            heFields[gasIndex].setText("00");
                        }
                        updateMODMND(gasIndex, o2Fields, heFields, modLabels, mndLabels, enabledBoxes, settings);
                    }
                }
            });
            mainPanel.add(gasPanel);
        }

        JPanel ppo2Panel = new JPanel(new FlowLayout(FlowLayout.LEFT, 8, 4));
        ppo2Panel.setBorder(BorderFactory.createTitledBorder("MOD PPO2"));
        ppo2Panel.add(new JLabel("底气 PPO2:"));
        final JTextField maxPPO2Field = new JTextField(String.valueOf(settings.getBottomPPO2()), 4);
        ppo2Panel.add(maxPPO2Field);
        ppo2Panel.add(new JLabel("上升 PPO2:"));
        final JTextField ascentPPO2Field = new JTextField(String.valueOf(settings.getAscentPPO2()), 4);
        ppo2Panel.add(ascentPPO2Field);

        if (settings.isTrimixMode()) {
            ppo2Panel.add(new JLabel("最大END:"));
            final JTextField maxEndField = new JTextField(String.valueOf(settings.getMaxEnd()), 4);
            ppo2Panel.add(maxEndField);
        }

        mainPanel.add(ppo2Panel);

        JPanel buttonPanel = new JPanel(new FlowLayout(FlowLayout.CENTER));
        JButton okButton = new JButton("确定");
        JButton cancelButton = new JButton("取消");

        final Runnable updateAllGasMODs = new Runnable() {
            @Override
            public void run() {
                try {
                    double bottomPPO2 = Double.parseDouble(maxPPO2Field.getText().trim());
                    if (bottomPPO2 >= 1.0 && bottomPPO2 <= 1.6) {
                        settings.setBottomPPO2(bottomPPO2);
                    }
                } catch (NumberFormatException ex) { }
                try {
                    double ascentPPO2 = Double.parseDouble(ascentPPO2Field.getText().trim());
                    if (ascentPPO2 >= 1.0 && ascentPPO2 <= 1.6) {
                        settings.setAscentPPO2(ascentPPO2);
                    }
                } catch (NumberFormatException ex) { }

                for (int i = 0; i < 3; i++) {
                    updateMODMND(i, o2Fields, heFields, modLabels, mndLabels, enabledBoxes, settings);
                }
            }
        };
        ActionListener ppo2UpdateListener = new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                updateAllGasMODs.run();
            }
        };
        maxPPO2Field.addActionListener(ppo2UpdateListener);
        ascentPPO2Field.addActionListener(ppo2UpdateListener);
        maxPPO2Field.addFocusListener(new FocusAdapter() {
            @Override
            public void focusLost(FocusEvent e) {
                updateAllGasMODs.run();
            }
        });
        ascentPPO2Field.addFocusListener(new FocusAdapter() {
            @Override
            public void focusLost(FocusEvent e) {
                updateAllGasMODs.run();
            }
        });

        okButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                for (int i = 0; i < 3; i++) {
                    Gas gas = settings.getGases()[i];
                    gas.setEnabled(enabledBoxes[i].isSelected());
                    try {
                        int o2 = Integer.parseInt(o2Fields[i].getText().trim());
                        if (settings.isTrimixMode()) {
                            if (o2 >= 8 && o2 <= 50) {
                                int he = 0;
                                try { he = Integer.parseInt(heFields[i].getText().trim()); } catch (Exception ex) {}
                                if (he >= 0 && he <= 90 && o2 + he <= 100) {
                                    gas.setTrimix(o2 / 100.0, he / 100.0);
                                }
                            }
                        } else {
                            if (o2 >= 21 && o2 <= 100) {
                                gas.setOxygenAmount(o2 / 100.0);
                            }
                        }
                    } catch (NumberFormatException ex) {}
                }

                updateAllGasMODs.run();

                boolean anyEnabled = false;
                for (Gas g : settings.getGases()) {
                    if (g.isEnabled()) { anyEnabled = true; break; }
                }
                if (!anyEnabled) { settings.getGases()[0].setEnabled(true); }

                zhl16.updateSettings(settings);
                updateCallback.run();
                dialog.dispose();

                System.out.println("=== 气体配置已更新 ===");
                for (int i = 0; i < 3; i++) {
                    Gas g = settings.getGases()[i];
                    System.out.printf("Gas %d: %s %s MOD=%.0fm",
                            i + 1, g.toString(),
                            g.isEnabled() ? "[启用]" : "[禁用]",
                            settings.calculateMODForGas(g));
                    if (g.isTrimix()) {
                        System.out.printf(" MND=%.0fm", g.calculateMND(settings.getMaxEnd()));
                    }
                    System.out.println();
                }
                System.out.printf("RMV: %.1f L/min%n", settings.getRmv());
            }
        });

        cancelButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) { dialog.dispose(); }
        });

        buttonPanel.add(okButton);
        buttonPanel.add(cancelButton);

        dialog.add(mainPanel, BorderLayout.CENTER);
        dialog.add(buttonPanel, BorderLayout.SOUTH);
        dialog.setVisible(true);
    }

    private static void updateMODMND(int gasIndex, JTextField[] o2Fields, JTextField[] heFields,
                                     JLabel[] modLabels, JLabel[] mndLabels,
                                     JCheckBox[] enabledBoxes, DiveSettings settings) {
        try {
            int o2 = Integer.parseInt(o2Fields[gasIndex].getText().trim());
            int he = 0;
            if (heFields[gasIndex] != null) {
                he = Integer.parseInt(heFields[gasIndex].getText().trim());
            }

            boolean validInput = false;
            if (settings.isTrimixMode()) {
                validInput = o2 >= 8 && o2 <= 50 && he >= 0 && he <= 90 && (o2 + he) <= 100;
            } else {
                validInput = o2 >= 21 && o2 <= 100;
            }

            if (validInput && enabledBoxes[gasIndex].isSelected()) {
                Gas gas = settings.getGases()[gasIndex];

                if (settings.isTrimixMode()) {
                    gas.setTrimix(o2 / 100.0, he / 100.0);
                } else {
                    gas.setOxygenAmount(o2 / 100.0);
                }

                gas.setMaxPPO2(settings.getPPO2LimitForGas(gas));

                double mod = settings.calculateMODForGas(gas);
                modLabels[gasIndex].setText(Math.round(mod) + "m");
                modLabels[gasIndex].setForeground(Color.BLACK);

                if (settings.isTrimixMode() && mndLabels[gasIndex] != null) {
                    double mnd = gas.calculateMND(settings.getMaxEnd());
                    mndLabels[gasIndex].setText((int)mnd + "m");
                    mndLabels[gasIndex].setForeground(Color.BLACK);
                }
            }
        } catch (NumberFormatException ex) { }
    }

    private static void showDecoPlanDialog(JFrame parent, final DiveSettings settings,
                                           final ZHL16 zhl16, final PressureSensor pressureSensor) {
        DecoPlanWizard wizard = new DecoPlanWizard(parent, settings, zhl16);
        wizard.setVisible(true);
    }

    private static void showDecoTestDialog(JFrame parent, DiveSettings settings) {
        JDialog dialog = new JDialog(parent, "减压测试", true);
        dialog.setLayout(new BorderLayout());
        dialog.setSize(800, 700);
        dialog.setLocationRelativeTo(parent);

        JPanel inputPanel = new JPanel(new GridLayout(0, 2, 8, 8));
        inputPanel.setBorder(BorderFactory.createTitledBorder("潜水参数"));

        JLabel depthLabel = new JLabel("底部深度 (米):");
        final JTextField depthField = new JTextField("40", 6);
        inputPanel.add(depthLabel); inputPanel.add(depthField);

        JLabel timeLabel = new JLabel("底部时间 (分钟):");
        final JTextField timeField = new JTextField("25", 6);
        inputPanel.add(timeLabel); inputPanel.add(timeField);

        JLabel o2Label = new JLabel("氧气浓度 O2%:");
        final JTextField o2Field = new JTextField("21", 6);
        inputPanel.add(o2Label); inputPanel.add(o2Field);

        JLabel gfLowLabel = new JLabel("GF Low (%):");
        final JTextField gfLowField = new JTextField("30", 6);
        inputPanel.add(gfLowLabel); inputPanel.add(gfLowField);

        JLabel gfHighLabel = new JLabel("GF High (%):");
        final JTextField gfHighField = new JTextField("70", 6);
        inputPanel.add(gfHighLabel); inputPanel.add(gfHighField);

        JLabel descentLabel = new JLabel("下降速度 (米/分):");
        final JTextField descentField = new JTextField("20", 6);
        inputPanel.add(descentLabel); inputPanel.add(descentField);

        JLabel ascentLabel = new JLabel("上升速度 (米/分):");
        final JTextField ascentField = new JTextField("10", 6);
        inputPanel.add(ascentLabel); inputPanel.add(ascentField);

        JLabel ppo2Label = new JLabel("底部 PPO2:");
        final JTextField ppo2Field = new JTextField("1.4", 6);
        inputPanel.add(ppo2Label); inputPanel.add(ppo2Field);

        JLabel ascentPPO2Label = new JLabel("上升 PPO2:");
        final JTextField ascentPPO2Field = new JTextField("1.6", 6);
        inputPanel.add(ascentPPO2Label); inputPanel.add(ascentPPO2Field);

        final JTextArea textArea = new JTextArea();
        textArea.setEditable(false);
        JScrollPane scrollPane = new JScrollPane(textArea);
        scrollPane.setBorder(BorderFactory.createTitledBorder("计算结果"));

        JPanel buttonPanel = new JPanel(new FlowLayout(FlowLayout.CENTER));
        JButton calculateButton = new JButton("计算");
        JButton closeButton = new JButton("关闭");

        calculateButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                try {
                    double depth = Double.parseDouble(depthField.getText().trim());
                    double time = Double.parseDouble(timeField.getText().trim());
                    double o2 = Double.parseDouble(o2Field.getText().trim());
                    int gfLow = Integer.parseInt(gfLowField.getText().trim());
                    int gfHigh = Integer.parseInt(gfHighField.getText().trim());
                    double descentRate = Double.parseDouble(descentField.getText().trim());
                    double ascentRate = Double.parseDouble(ascentField.getText().trim());
                    double ppo2 = Double.parseDouble(ppo2Field.getText().trim());
                    double ascentPPO2 = Double.parseDouble(ascentPPO2Field.getText().trim());

                    DivePlanComparator comparator = new DivePlanComparator();
                    comparator.setBottomDepthMeter(depth);
                    comparator.setBottomTimeMinutes(time);
                    comparator.setGas1O2(o2);
                    comparator.setGfLow(gfLow);
                    comparator.setGfHigh(gfHigh);
                    comparator.setDescentRate(descentRate);
                    comparator.setAscentRate(ascentRate);
                    comparator.setBottomPPO2(ppo2);
                    comparator.setAscentPPO2(ascentPPO2);
                    comparator.setDecoPPO2(1.6);

                    String result = comparator.runTestSimple();
                    textArea.setText(result);
                } catch (NumberFormatException ex) {
                    JOptionPane.showMessageDialog(dialog, "请输入有效的数字！", "输入错误", JOptionPane.ERROR_MESSAGE);
                }
            }
        });

        closeButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) { dialog.dispose(); }
        });

        buttonPanel.add(calculateButton);
        buttonPanel.add(closeButton);

        dialog.add(inputPanel, BorderLayout.NORTH);
        dialog.add(scrollPane, BorderLayout.CENTER);
        dialog.add(buttonPanel, BorderLayout.SOUTH);
        dialog.setVisible(true);
    }

    private static void showNDLTestDialog(JFrame parent, DiveSettings settings) {
        JDialog dialog = new JDialog(parent, "NDL 无减压限制表格", true);
        dialog.setLayout(new BorderLayout());
        dialog.setSize(550, 500);
        dialog.setLocationRelativeTo(parent);

        JPanel inputPanel = new JPanel(new GridLayout(0, 2, 8, 8));
        inputPanel.setBorder(BorderFactory.createTitledBorder("NDL 表格参数"));

        JLabel o2Label = new JLabel("氧气浓度 O2%:");
        final JTextField o2Field = new JTextField("21", 6);
        inputPanel.add(o2Label); inputPanel.add(o2Field);

        JLabel gfLowLabel = new JLabel("GF Low (%):");
        final JTextField gfLowField = new JTextField("30", 6);
        inputPanel.add(gfLowLabel); inputPanel.add(gfLowField);

        JLabel gfHighLabel = new JLabel("GF High (%):");
        final JTextField gfHighField = new JTextField("70", 6);
        inputPanel.add(gfHighLabel); inputPanel.add(gfHighField);

        JLabel ppo2Label = new JLabel("最大 PPO2:");
        final JTextField ppo2Field = new JTextField("1.4", 6);
        inputPanel.add(ppo2Label); inputPanel.add(ppo2Field);

        final JTextArea textArea = new JTextArea();
        textArea.setEditable(false);
        JScrollPane scrollPane = new JScrollPane(textArea);

        JPanel buttonPanel = new JPanel(new FlowLayout(FlowLayout.CENTER));
        JButton calculateButton = new JButton("生成表格");
        JButton closeButton = new JButton("关闭");

        calculateButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                try {
                    double o2 = Double.parseDouble(o2Field.getText().trim());
                    int gfLow = Integer.parseInt(gfLowField.getText().trim());
                    int gfHigh = Integer.parseInt(gfHighField.getText().trim());
                    double maxPPO2 = Double.parseDouble(ppo2Field.getText().trim());

                    if (o2 <= 0 || o2 > 100) {
                        JOptionPane.showMessageDialog(dialog, "氧气浓度必须在 1-100 之间！", "输入错误", JOptionPane.ERROR_MESSAGE);
                        return;
                    }

                    double mod = (maxPPO2 / (o2 / 100.0) - 1) * 10;
                    String gasName = (o2 == 21) ? "Air" : String.format("%.0f%% O2", o2);

                    StringBuilder sb = new StringBuilder();
                    sb.append("\n======================================================================\n");
                    sb.append("                    NDL (无减压限制) 表格\n");
                    sb.append("======================================================================\n\n");
                    sb.append(String.format("  气体:        %s\n", gasName));
                    sb.append(String.format("  GF 设置:     %d/%d\n", gfLow, gfHigh));
                    sb.append(String.format("  最大 PPO2:   %.1f bar\n", maxPPO2));
                    sb.append(String.format("  最大深度:    %.0f 米 (自动计算)\n", mod));
                    sb.append("======================================================================\n\n");
                    sb.append(String.format("%-12s %-12s %-12s\n", "深度(米)", "NDL(分钟)", "气体"));
                    sb.append("----------------------------------------------------------\n");

                    DiveSettings testSettings = new DiveSettings();
                    GradientFactors gf = new GradientFactors(
                            gfLow / 100.0, gfHigh / 100.0, testSettings.getSurfacePressure()
                    );
                    testSettings.setGf(gf);
                    testSettings.setBottomPPO2(maxPPO2);
                    testSettings.setAscentPPO2(1.6);
                    testSettings.setDecoPPO2(1.6);

                    for (int depth = 12; depth <= mod; depth += 3) {
                        double depthBar = testSettings.getSurfacePressure() + depth / testSettings.getDepthPerBar();
                        ZHL16 zhl16 = new ZHL16(testSettings, 0);

                        int ndlSeconds = 0;
                        for (int sec = 0; sec < 43200; sec++) {
                            DiveDataPoint point = zhl16.dive(depthBar, 1);
                            ndlSeconds++;
                            if (point.getNdl() <= 0) break;
                        }
                        int ndlMin = ndlSeconds / 60;
                        sb.append(String.format("%-12d %-12d %-12s\n", depth, ndlMin, gasName));
                    }
                    sb.append("======================================================================\n");
                    textArea.setText(sb.toString());

                } catch (NumberFormatException ex) {
                    JOptionPane.showMessageDialog(dialog, "请输入有效的数字！", "输入错误", JOptionPane.ERROR_MESSAGE);
                }
            }
        });

        closeButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) { dialog.dispose(); }
        });

        buttonPanel.add(calculateButton);
        buttonPanel.add(closeButton);

        dialog.add(inputPanel, BorderLayout.NORTH);
        dialog.add(scrollPane, BorderLayout.CENTER);
        dialog.add(buttonPanel, BorderLayout.SOUTH);
        dialog.setVisible(true);
    }
}

class DiveSettings {
    public enum WaterDensity {
        FRESH("Fresh Water", 10.0),
        SALT("Salt Water", 9.75),
        EN13319("EN13319", 9.80);

        private final String label;
        private final double metersPerBar;

        WaterDensity(String label, double metersPerBar) {
            this.label = label;
            this.metersPerBar = metersPerBar;
        }

        public double getMetersPerBar() { return metersPerBar; }

        @Override
        public String toString() { return label; }
    }

    private double surfacePressure;
    private double depthPerBar;
    private WaterDensity waterDensity = WaterDensity.EN13319;
    private GradientFactors gf;
    private Gas gas = new Gas();
    private double maxPP02;
    private double minStopTime;

    private Gas[] gases = new Gas[3];
    private int activeGasIndex = 0;
    private double decoPPO2 = 1.6;
    private double bottomPPO2 = 1.4;
    private double ascentPPO2 = 1.6;
    private int lastStopDepth = 3;
    private boolean multiGasMode = false;
    private double rmv = 14.0;

    private boolean trimixMode = false;
    private double maxEnd = 30.0;
    private double trimixO2 = 0.18;
    private double trimixHe = 0.45;

    private double maxAscentRate;

    private final static double flightPressure = 0.58;
    private final static double Pw = 0.0627;

    public int [] stops = {  0,  3,  6,  9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39,
            42, 45, 48, 51, 54, 57, 60, 63, 66, 69, 72, 75,
            78, 81, 84, 87, 90, 93, 96, 99, 102, 105, 108, 111,
            114, 117, 120};

    private double logThreshold;

    public DiveSettings(){
        this(1.0, WaterDensity.EN13319.getMetersPerBar(), new GradientFactors(1), new Gas(), 1.4);
    }

    public DiveSettings(double surfacePressure, double depthPerBar, GradientFactors gf, Gas gas, double maxPP02) {
        this.surfacePressure = surfacePressure;
        this.depthPerBar = depthPerBar;
        this.gf = new GradientFactors(gf.getLow(), gf.getHigh(), this.surfacePressure);
        this.gas = gas;
        this.maxPP02 = maxPP02;
        logThreshold = 0.1;
        maxAscentRate = 0.3;
        minStopTime = 0;
        initializeGases();
        applyPPO2LimitsToGases();
    }

    private void initializeGases() {
        gases[0] = new Gas(0.21);
        gases[0].setMaxPPO2(bottomPPO2);
        gases[0].setEnabled(true);
        gases[0].setName("1");

        gases[1] = new Gas(0.21);
        gases[1].setMaxPPO2(decoPPO2);
        gases[1].setEnabled(false);
        gases[1].setName("2");

        gases[2] = new Gas(0.21);
        gases[2].setMaxPPO2(decoPPO2);
        gases[2].setEnabled(false);
        gases[2].setName("3");
    }

    public void setSurfacePressure(double surfacePressure) {
        if (surfacePressure > 0.45 && surfacePressure < 1.2) {
            this.surfacePressure = surfacePressure;
            this.gf = new GradientFactors(gf.getLow(), gf.getHigh(), this.surfacePressure);
        }
    }
    public void setDepthPerBar(double depthPerBar) { this.depthPerBar = depthPerBar; }
    public void setWaterDensity(WaterDensity density) {
        if (density != null) {
            this.waterDensity = density;
            this.depthPerBar = density.getMetersPerBar();
        }
    }
    public WaterDensity getWaterDensity() { return waterDensity; }
    public static double altitudeToBar(double altitudeMeters) {
        return Math.exp(-altitudeMeters / 8434.4);
    }

    public void setGf(GradientFactors gf) {
        if (gf.getHigh()>1) gf.setHigh(1);
        if (gf.getHigh()<0.01) gf.setHigh(0.01);
        if (gf.getLow()>1) gf.setLow(1);
        if (gf.getLow()<0.01) gf.setLow(0.01);
        this.gf = gf;
    }

    public void setGas(Gas gas) {
        this.gas = gas;
        if (this.gas != null) this.gas.setMaxPPO2(getPPO2LimitForGas(this.gas));
    }
    public void setMaxPP02(double maxPP02) { this.maxPP02 = maxPP02; }
    public static double getFlightPressure() { return flightPressure; }
    public static double getPw() { return Pw; }
    public double getDepthPerBar() { return depthPerBar; }
    public GradientFactors getGf() { return gf; }
    public Gas getGas() { return gas; }
    public double getMaxPP02() { return maxPP02; }
    public double getSurfacePressure() { return surfacePressure; }
    public double getLogThreshold() { return logThreshold; }
    public void setLogThreshold(double threshold) { logThreshold = threshold; }
    public double getMaxAscentRate() { return maxAscentRate; }
    public void setMaxAscentRate(double maxAscentRate) { this.maxAscentRate = maxAscentRate; }
    public double getMinStopTime() { return minStopTime; }
    public void setMinStopTime(double minStopTime) { this.minStopTime = minStopTime; }

    public Gas getActiveGas() {
        if (multiGasMode) return gases[activeGasIndex];
        return gas;
    }

    public void setActiveGasIndex(int index) {
        if (index >= 0 && index < 3 && gases[index].isEnabled()) {
            this.activeGasIndex = index;
        }
    }

    public int getActiveGasIndex() { return activeGasIndex; }
    public Gas[] getGases() { return gases; }

    public void setGasAt(int index, Gas gas) {
        if (index >= 0 && index < 3) {
            gases[index] = gas;
            if (gas != null) gas.setMaxPPO2(getPPO2LimitForGas(gas));
        }
    }

    public double getPPO2LimitForGas(Gas gas) {
        if (multiGasMode && gas != null) {
            int gasIndex = getGasIndex(gas);
            if (gasIndex >= 0 && gasIndex != getBottomGasIndex()) return ascentPPO2;
        }
        if (gas != null && gas.getOxygenAmount() >= 0.50) return decoPPO2;
        return bottomPPO2;
    }

    public double calculateMODForGas(Gas gas) {
        if (gas == null) return 0;
        double oxygen = gas.getOxygenAmount();
        if (oxygen <= 0) return Double.MAX_VALUE;
        double modBar = getPPO2LimitForGas(gas) / oxygen;
        return Math.max(0, (modBar - surfacePressure) * depthPerBar);
    }

    public void applyPPO2LimitsToGases() {
        if (gas != null) gas.setMaxPPO2(getPPO2LimitForGas(gas));
        for (Gas g : gases) {
            if (g != null) g.setMaxPPO2(getPPO2LimitForGas(g));
        }
    }

    public Gas getBestGasForDepth(double depthMeter) {
        Gas bestGas = null;
        double highestO2 = 0;
        for (Gas g : gases) {
            if (g.isEnabled()) {
                double mod = calculateMODForGas(g);
                if (depthMeter <= mod && g.getOxygenAmount() > highestO2) {
                    highestO2 = g.getOxygenAmount();
                    bestGas = g;
                }
            }
        }
        return bestGas;
    }

    public int getBottomGasIndex() {
        int bestIndex = -1;
        double lowestO2 = Double.MAX_VALUE;
        for (int i = 0; i < 3; i++) {
            Gas g = gases[i];
            if (g != null && g.isEnabled() && g.getOxygenAmount() < lowestO2) {
                lowestO2 = g.getOxygenAmount();
                bestIndex = i;
            }
        }
        if (bestIndex >= 0) return bestIndex;
        if (activeGasIndex >= 0 && activeGasIndex < 3) return activeGasIndex;
        return 0;
    }

    public int getGasIndex(Gas gas) {
        for (int i = 0; i < 3; i++) {
            if (gases[i] == gas) return i;
        }
        return -1;
    }

    public int getBestHigherGasIndexForDepth(double depthMeter, int currentIndex) {
        double currentO2 = 0;
        if (currentIndex >= 0 && currentIndex < 3 && gases[currentIndex] != null) {
            currentO2 = gases[currentIndex].getOxygenAmount();
        }

        int bestIndex = -1;
        double highestO2 = currentO2;
        for (int i = 0; i < 3; i++) {
            Gas g = gases[i];
            if (g != null && g.isEnabled()) {
                double mod = calculateMODForGas(g);
                if (depthMeter <= mod + 0.01 && g.getOxygenAmount() > highestO2 + 0.0001) {
                    highestO2 = g.getOxygenAmount();
                    bestIndex = i;
                }
            }
        }
        return bestIndex;
    }

    public int getBestGasIndexForDepth(double depthMeter) {
        int bestIndex = activeGasIndex;
        double highestO2 = 0;
        for (int i = 0; i < 3; i++) {
            if (gases[i].isEnabled()) {
                double mod = calculateMODForGas(gases[i]);
                if (depthMeter <= mod && gases[i].getOxygenAmount() > highestO2) {
                    highestO2 = gases[i].getOxygenAmount();
                    bestIndex = i;
                }
            }
        }
        return bestIndex;
    }

    public boolean hasBetterGasAvailable(double depthMeter) {
        if (!multiGasMode) return false;
        Gas currentGas = gases[activeGasIndex];
        Gas bestGas = getBestGasForDepth(depthMeter);
        if (bestGas != null && bestGas != currentGas) {
            return bestGas.getOxygenAmount() > currentGas.getOxygenAmount();
        }
        return false;
    }

    public boolean canSwitchToGas(int gasIndex, double depthMeter) {
        if (gasIndex < 0 || gasIndex >= 3) return false;
        Gas targetGas = gases[gasIndex];
        if (!targetGas.isEnabled()) return false;
        double mod = calculateMODForGas(targetGas);
        return depthMeter <= mod;
    }

    public int getEnabledGasCount() {
        int count = 0;
        for (Gas g : gases) { if (g.isEnabled()) count++; }
        return count;
    }

    public boolean isMultiGasMode() { return multiGasMode; }

    public void setMultiGasMode(boolean multiGasMode) {
        this.multiGasMode = multiGasMode;
        if (multiGasMode) {
            gases[0] = gas;
            applyPPO2LimitsToGases();
        }
    }

    public double getDecoPPO2() { return decoPPO2; }

    public void setDecoPPO2(double decoPPO2) {
        this.decoPPO2 = decoPPO2;
        applyPPO2LimitsToGases();
    }

    public double getAscentPPO2() { return ascentPPO2; }

    public void setAscentPPO2(double ascentPPO2) {
        this.ascentPPO2 = ascentPPO2;
        applyPPO2LimitsToGases();
    }

    public double getBottomPPO2() { return bottomPPO2; }

    public void setBottomPPO2(double bottomPPO2) {
        this.bottomPPO2 = bottomPPO2;
        applyPPO2LimitsToGases();
    }

    public int getLastStopDepth() { return lastStopDepth; }

    public void setLastStopDepth(int lastStopDepth) {
        if (lastStopDepth == 3 || lastStopDepth == 6) this.lastStopDepth = lastStopDepth;
    }

    public double getRmv() { return rmv; }

    public void setRmv(double rmv) {
        if (rmv > 0 && rmv <= 50) this.rmv = rmv;
    }

    public double calculateGasConsumption(double depthMeter, double timeMinutes) {
        double pressureATA = surfacePressure + depthMeter / depthPerBar;
        return rmv * timeMinutes * pressureATA;
    }

    public boolean isTrimixMode() { return trimixMode; }

    public void setTrimixMode(boolean trimixMode) {
        this.trimixMode = trimixMode;
        if (trimixMode) {
            this.multiGasMode = true;
            gases[0] = new Gas(trimixO2, trimixHe);
            gases[0].setMaxPPO2(getPPO2LimitForGas(gases[0]));
            gases[0].setEnabled(true);
            gases[0].setName("1");
            this.gas = gases[0];
            this.activeGasIndex = 0;
        }
    }

    public double getMaxEnd() { return maxEnd; }

    public void setMaxEnd(double maxEnd) {
        if (maxEnd > 0 && maxEnd <= 100) this.maxEnd = maxEnd;
    }

    public double getTrimixO2() { return trimixO2; }

    public void setTrimixO2(double trimixO2) {
        if (trimixO2 >= 0.08 && trimixO2 <= 0.50) {
            this.trimixO2 = trimixO2;
            if (trimixMode) {
                gases[0] = new Gas(trimixO2, trimixHe);
                gases[0].setMaxPPO2(getPPO2LimitForGas(gases[0]));
                gases[0].setEnabled(true);
                gases[0].setName("1");
                this.gas = gases[0];
            }
        }
    }

    public double getTrimixHe() { return trimixHe; }

    public void setTrimixHe(double trimixHe) {
        if (trimixHe >= 0 && trimixHe <= 0.90) {
            this.trimixHe = trimixHe;
            if (trimixMode) {
                gases[0] = new Gas(trimixO2, trimixHe);
                gases[0].setMaxPPO2(getPPO2LimitForGas(gases[0]));
                gases[0].setEnabled(true);
                gases[0].setName("1");
                this.gas = gases[0];
            }
        }
    }

    public String getTrimixInfo() {
        Gas activeGas = getActiveGas();
        if (activeGas != null && activeGas.isTrimix()) {
            return activeGas.getTrimixInfo(surfacePressure, depthPerBar, maxEnd);
        }
        return "";
    }
}

class Gas {
    private double oxygenAmount;
    private double heAmount;
    private double n2Amount;
    private boolean enabled;
    private double maxPPO2;
    private String name;
    private static final double DEFAULT_MAX_END = 30.0;

    public Gas() { this(0.21); }

    public Gas(double oxygenAmount) { this(oxygenAmount, 0); }

    public Gas(double oxygenAmount, double heAmount) {
        this.oxygenAmount = oxygenAmount;
        this.heAmount = heAmount;
        this.n2Amount = 1 - this.oxygenAmount - this.heAmount;
        this.enabled = true;
        this.maxPPO2 = 1.4;
        this.name = generateName();
    }

    public double calculateMOD(double surfacePressure, double depthPerBar) {
        if (oxygenAmount <= 0) return Double.MAX_VALUE;
        double modBar = maxPPO2 / oxygenAmount;
        double modMeter = (modBar - surfacePressure) * depthPerBar;
        return Math.max(0, modMeter);
    }

    public double calculateMODRounded(double surfacePressure, double depthPerBar) {
        double mod = calculateMOD(surfacePressure, depthPerBar);
        return Math.floor(mod / 3) * 3;
    }

    public boolean isSafeAtDepth(double depthMeter, double surfacePressure, double depthPerBar) {
        return depthMeter <= calculateMOD(surfacePressure, depthPerBar);
    }

    public double getPPO2AtDepth(double depthMeter, double surfacePressure, double depthPerBar) {
        double pressureBar = surfacePressure + depthMeter / depthPerBar;
        return oxygenAmount * pressureBar;
    }

    public boolean isTrimix() { return heAmount > 0; }

    public boolean isHelitrox() { return isTrimix() && oxygenAmount >= 0.21; }

    public double calculateEAD(double depthMeter, double surfacePressure, double depthPerBar) {
        double ambientPressure = surfacePressure + depthMeter / depthPerBar;
        double fN2 = n2Amount;
        if (fN2 <= 0) return 0;
        double eadBar = (ambientPressure * fN2) / 0.79;
        return (eadBar - surfacePressure) * depthPerBar;
    }

    public double calculateEND(double depthMeter) {
        double narcoticIndex = n2Amount + oxygenAmount;
        return depthMeter * narcoticIndex;
    }

    public double calculateMND(double narcoticDepthMeters) {
        double narcoticIndex = n2Amount + oxygenAmount;
        if (narcoticIndex <= 0) return Double.MAX_VALUE;
        return narcoticDepthMeters / narcoticIndex;
    }

    public double calculateDensity() {
        double densityN2 = 1.251 * n2Amount;
        double densityO2 = 1.428 * oxygenAmount;
        double densityHe = 0.179 * heAmount;
        return densityN2 + densityO2 + densityHe;
    }

    public double getNarcoticIndex() { return n2Amount + oxygenAmount; }

    public boolean isSafeForNarcosis(double depthMeter, double maxNarcoticDepth) {
        return calculateEND(depthMeter) <= maxNarcoticDepth;
    }

    public String getTrimixInfo(double surfacePressure, double depthPerBar, double maxNarcoticDepth) {
        if (!isTrimix()) {
            return String.format("EAN%d MOD: %.0fm",
                    (int)(oxygenAmount * 100), calculateMOD(surfacePressure, depthPerBar));
        }
        double mod = calculateMOD(surfacePressure, depthPerBar);
        double switchDepth = calculateMODRounded(surfacePressure, depthPerBar);
        double mnd = calculateMND(maxNarcoticDepth);

        return String.format("Tx%d/%02d MOD: %.0fm, Switch: %.0fm, MND: %.0fm",
                (int)(oxygenAmount * 100), (int)(heAmount * 100), mod, switchDepth, mnd);
    }

    private String generateName() {
        int o2Percent = (int) Math.round(oxygenAmount * 100);
        if (o2Percent == 21 && heAmount == 0) {
            return "Air";
        } else if (o2Percent == 100) {
            return "O2";
        } else if (heAmount == 0) {
            return "EAN" + o2Percent;
        } else {
            int hePercent = (int) Math.round(heAmount * 100);
            return "Tx" + o2Percent + "/" + hePercent;
        }
    }

    public double getN2Amount() { return n2Amount; }
    public double getHeAmount() { return heAmount; }
    public double getOxygenAmount() { return oxygenAmount; }
    public boolean isEnabled() { return enabled; }
    public double getMaxPPO2() { return maxPPO2; }
    public String getName() { return name; }

    public void setEnabled(boolean enabled) { this.enabled = enabled; }
    public void setMaxPPO2(double maxPPO2) { this.maxPPO2 = maxPPO2; }
    public void setOxygenAmount(double oxygenAmount) {
        this.oxygenAmount = oxygenAmount;
        this.n2Amount = 1 - this.oxygenAmount - this.heAmount;
        this.name = generateName();
    }
    public void setHeAmount(double heAmount) {
        this.heAmount = heAmount;
        this.n2Amount = 1 - this.oxygenAmount - this.heAmount;
        this.name = generateName();
    }
    public void setName(String name) { this.name = name; }
    public void setTrimix(double oxygenAmount, double heAmount) {
        this.oxygenAmount = oxygenAmount;
        this.heAmount = heAmount;
        this.n2Amount = 1 - this.oxygenAmount - this.heAmount;
        this.name = generateName();
    }

    @Override
    public String toString() {
        if (heAmount > 0) {
            return name + " (" + Math.round(oxygenAmount * 100) + "% O2, " + Math.round(heAmount * 100) + "% He)";
        } else {
            return name + " (" + Math.round(oxygenAmount * 100) + "% O2)";
        }
    }
}

class GradientFactors {
    private double low;
    private double high;
    private double highDepth;
    private double lowDepth;

    public GradientFactors(double surfacePressure){
        this(0.3, 0.8, surfacePressure);
    }

    public GradientFactors(double gfLow, double gfHigh, double surfacePressure){
        this.low = gfLow;
        this.high = gfHigh;
        this.highDepth = surfacePressure;
        this.lowDepth = surfacePressure + 0.1;
    }

    public void setLow(double low) { this.low = low; }
    public void setHigh(double high) { this.high = high; }
    public double getLow() { return low; }
    public double getHigh() { return high; }

    public double getGF(double depthInBar){
        if (lowDepth <= highDepth) return high;
        if (depthInBar >= lowDepth) return low;
        if (depthInBar <= highDepth) return high;

        double depthRange = lowDepth - highDepth;
        double depthFromLow = lowDepth - depthInBar;
        double ratio = depthFromLow / depthRange;
        return low + (high - low) * ratio;
    }

    public double gfCorrectA(double depthInBar, double a){ return getGF(depthInBar)*a; }
    public double gfCorrectB(double depthInBar, double b){ return 1/((getGF(depthInBar)/b)-getGF(depthInBar)+1); }
    public void setHighDepth(double highDepth) { this.highDepth = highDepth; }
    public void setLowDepth(double lowDepth) { this.lowDepth = lowDepth; }
    public double getLowDepth() { return lowDepth; }
}

class Dive {
    private int startTime;
    private double maxDepth;
    private LinkedList profile = new LinkedList();
    public DiveSettings settings;

    public Dive(DiveSettings settings){
        this.maxDepth = 0;
        this.settings = settings;
        profile.addFirst(new DiveDataPoint(0, settings.getSurfacePressure()));
    }

    public void setPoint(DiveDataPoint point){
        DiveDataPoint lastLogged = (DiveDataPoint) profile.getFirst();
        if ((point.getDepthInBar()-lastLogged.getDepthInBar())>(settings.getLogThreshold()/2) ||
            (point.getDepthInBar()-lastLogged.getDepthInBar())<(-settings.getLogThreshold()/2)){
            profile.addFirst(point);
        }
    }
}

class DiveDataPoint {
    private double time;
    private double depthInBar;
    private double ndl;
    private double stopDepth;
    private double stopTime;
    private double tts;
    private int leadingTissue;
    private double GF;
    private double maxDepth;
    private double temperature;

    public DiveDataPoint(){this(0,0);}
    public DiveDataPoint(double time, double depthInBar){
        this(time, depthInBar, 5940, -1, -1, -1, -1, -1, -1, -1);
    }
    public DiveDataPoint(double time, double depthInBar, double ndl, double stopDepth, double stopTime, double tts, int leadingTissue, double GF, double maxDepth, double temperature) {
        this.time = time;
        this.depthInBar = depthInBar;
        this.ndl = ndl;
        this.stopDepth = stopDepth;
        this.stopTime = stopTime;
        this.tts = tts;
        this.leadingTissue = leadingTissue;
        this.GF = GF;
        this.maxDepth = maxDepth;
        this.temperature = temperature;
    }
    public double getTime() { return time; }
    public double getDepthInBar() { return depthInBar; }
    public double getNdl() { return ndl; }
    public void setTime(long time) { this.time = time; }
    public void setDepthInBar(double depthInBar) { this.depthInBar = depthInBar; }
    public void setNdl(double ndl) { this.ndl = ndl; }
    public double getStopDepth() { return stopDepth; }
    public double getStopTime() { return stopTime; }
    public double getTts() { return tts; }
    public int getLeadingTissue() { return leadingTissue; }
    public void setLeadingTissue(int leadingTissue) { this.leadingTissue = leadingTissue; }
    public double getGF() { return GF; }
    public void setGF(double GF) { this.GF = GF; }
    public double getMaxDepth() { return maxDepth; }
    public void setMaxDepth(double maxDepth) { this.maxDepth = maxDepth; }
}

class SafetyStop {
    private double stopDepth;
    private double stopTime;
    private boolean isActive;
    private Gas gas;
    private double gasQty;

    public SafetyStop(double stopDepth) {
        this.stopDepth = stopDepth;
        stopTime = 0;
        isActive = false;
        gas = null;
        gasQty = 0;
    }
    public boolean isActive() { return isActive; }
    public void setActive(boolean active) { isActive = active; }
    public double getStopDepth() { return stopDepth; }
    public void setStopDepth(double stopDepth) { this.stopDepth = stopDepth; }
    public double getStopTime() { return stopTime; }
    public void setStopTime(double stopTime) { this.stopTime = stopTime; }
    public Gas getGas() { return gas; }
    public void setGas(Gas gas) { this.gas = gas; }
    public double getGasQty() { return gasQty; }
    public void setGasQty(double gasQty) { this.gasQty = gasQty; }
}

class Tissue {
    private double a;
    private double b;
    private double k;
    private double heA;
    private double heB;
    private double heK;
    private double pN2;
    private double pHe;

    public Tissue(double a, double b, double k, double initialLoad) {
        this(a, b, k, 0, 0, 0, initialLoad, initialLoad);
    }
    public Tissue(double a, double b, double k, double heA, double heB, double heK,
                  double initialN2Load, double initialHeLoad) {
        this.a = a;
        this.b = b;
        this.k = k;
        this.heA = heA;
        this.heB = heB;
        this.heK = heK;
        this.pN2 = initialN2Load;
        this.pHe = initialHeLoad;
    }
    public double getA() { return a; }
    public double getB() { return b; }
    public double getK() { return k; }
    public double getPN2() { return pN2; }
    public void setPN2(double pN2) { this.pN2 = pN2; }
    public double getHeA() { return heA; }
    public double getHeB() { return heB; }
    public double getHeK() { return heK; }
    public double getPHe() { return pHe; }
    public void setPHe(double pHe) { this.pHe = pHe; }
    public double getTotalLoad() { return pN2 + pHe; }
    public double getMixedA() {
        double total = getTotalLoad();
        if (total <= 0) return a;
        return (a * pN2 + heA * pHe) / total;
    }
    public double getMixedB() {
        double total = getTotalLoad();
        if (total <= 0) return b;
        return (b * pN2 + heB * pHe) / total;
    }
    public double getMixedMValue(double ambientPressure) {
        return getMixedA() + ambientPressure / getMixedB();
    }
    public void setLoad(double load) { this.pN2 = load; }
    public double getLoad() { return getTotalLoad(); }
}

class TissueModel {
    public static final double LOG2_60 = 0.01155245301;
    public Tissue[] n2Compartments = new Tissue[16];

    public TissueModel(double n2InitialLoad) {
        this(n2InitialLoad, 0);
    }
    public TissueModel(double n2InitialLoad, double heInitialLoad) {
        n2Compartments[0]  = new Tissue( 1.1696, 0.5578, 0.00231049060186648,
                1.6189, 0.4770, 0.00614598235224574, n2InitialLoad, heInitialLoad);
        n2Compartments[1]  = new Tissue( 1.0000, 0.6514, 0.00144405662616655,
                1.3830, 0.5747, 0.00382663477119536, n2InitialLoad, heInitialLoad);
        n2Compartments[2]  = new Tissue( 0.8618, 0.7222, 0.00092419624074659,
                1.1919, 0.6527, 0.00244882027754237, n2InitialLoad, heInitialLoad);
        n2Compartments[3]  = new Tissue( 0.7562, 0.7826, 0.00062445691942338,
                1.0458, 0.7223, 0.00165387312015737, n2InitialLoad, heInitialLoad);
        n2Compartments[4]  = new Tissue( 0.6200, 0.8125, 0.00042786862997528,
                0.9220, 0.7582, 0.00113167926542605, n2InitialLoad, heInitialLoad);
        n2Compartments[5]  = new Tissue( 0.5043, 0.8434, 0.00030163062687552,
                0.8205, 0.7957, 0.00079837463647003, n2InitialLoad, heInitialLoad);
        n2Compartments[6]  = new Tissue( 0.4410, 0.8693, 0.00021275235744627,
                0.7305, 0.8279, 0.00056329682834731, n2InitialLoad, heInitialLoad);
        n2Compartments[7]  = new Tissue( 0.4000, 0.8910, 0.00015003185726406,
                0.6502, 0.8553, 0.00039709542229509, n2InitialLoad, heInitialLoad);
        n2Compartments[8]  = new Tissue( 0.3750, 0.9092, 0.00010598580742507,
                0.5950, 0.8757, 0.00028040031895971, n2InitialLoad, heInitialLoad);
        n2Compartments[9]  = new Tissue( 0.3500, 0.9222, 0.00007912639047488,
                0.5545, 0.8903, 0.00020925595568824, n2InitialLoad, heInitialLoad);
        n2Compartments[10] = new Tissue( 0.3295, 0.9319, 0.00006177782357932,
                0.5333, 0.8997, 0.00016340648576634, n2InitialLoad, heInitialLoad);
        n2Compartments[11] = new Tissue( 0.3065, 0.9403, 0.00004833662347001,
                0.5189, 0.9073, 0.00012795529897048, n2InitialLoad, heInitialLoad);
        n2Compartments[12] = new Tissue( 0.2835, 0.9477, 0.00003787689511257,
                0.5181, 0.9122, 0.00010027769956597, n2InitialLoad, heInitialLoad);
        n2Compartments[13] = new Tissue( 0.2610, 0.9544, 0.00002962167438290,
                0.5176, 0.9171, 0.00007838195292259, n2InitialLoad, heInitialLoad);
        n2Compartments[14] = new Tissue( 0.2480, 0.9602, 0.00002319769680589,
                0.5172, 0.9217, 0.00006139453800720, n2InitialLoad, heInitialLoad);
        n2Compartments[15] = new Tissue( 0.2327, 0.9653, 0.00001819283938478,
                0.5119, 0.9267, 0.00004813085420624, n2InitialLoad, heInitialLoad);
    }
    public static double calculateHeK(double halfTimeMinutes) { return LOG2_60 / halfTimeMinutes; }
    public static double calculateN2K(double halfTimeMinutes) { return LOG2_60 / halfTimeMinutes; }
}

class ZHL16 {
    private TissueModel tissueModel;
    private DiveDataPoint lastPoint;
    private DiveSettings diveSettings;
    private double maxDepth;
    private SafetyStop[] stops;

    public ZHL16(DiveSettings settings, long currentTimeStamp){
        this.diveSettings = new DiveSettings(settings.getSurfacePressure(), settings.getDepthPerBar(), settings.getGf(), settings.getGas(), settings.getMaxPP02());
        this.diveSettings.setWaterDensity(settings.getWaterDensity());
        this.diveSettings.setSurfacePressure(settings.getSurfacePressure());
        this.diveSettings.setBottomPPO2(settings.getBottomPPO2());
        this.diveSettings.setAscentPPO2(settings.getAscentPPO2());
        this.diveSettings.setDecoPPO2(settings.getDecoPPO2());
        this.diveSettings.setLastStopDepth(settings.getLastStopDepth());
        this.diveSettings.setRmv(settings.getRmv());
        this.diveSettings.setMaxEnd(settings.getMaxEnd());
        this.diveSettings.setTrimixO2(settings.getTrimixO2());
        this.diveSettings.setTrimixHe(settings.getTrimixHe());
        this.diveSettings.setTrimixMode(settings.isTrimixMode());
        this.diveSettings.setMultiGasMode(settings.isMultiGasMode());
        if (settings.isMultiGasMode()) {
            Gas[] sourceGases = settings.getGases();
            for (int i = 0; i < sourceGases.length; i++) {
                Gas src = sourceGases[i];
                Gas dst = new Gas(src.getOxygenAmount(), src.getHeAmount());
                dst.setMaxPPO2(src.getMaxPPO2());
                dst.setEnabled(src.isEnabled());
                dst.setName(src.getName());
                this.diveSettings.setGasAt(i, dst);
            }
            this.diveSettings.setActiveGasIndex(settings.getActiveGasIndex());
        }
        double initialN2 = getPalvN2(diveSettings.getSurfacePressure());
        double initialHe = getPalvHe(diveSettings.getSurfacePressure());
        this.tissueModel = new TissueModel(initialN2, initialHe);
        this.lastPoint = new DiveDataPoint(currentTimeStamp, settings.getSurfacePressure());
        stops = new SafetyStop[40];
        updateSafetyStops();
    }

    public void updateSettings(DiveSettings settings){
        this.diveSettings = settings;
        updateSafetyStops();
    }

    private void updateSafetyStops(){
        for (int i=0; i<40;i++){
            stops[i]= new SafetyStop(meterToBar(diveSettings.stops[i], diveSettings));
        }
    }
    public DiveSettings getDiveSettings(){ return diveSettings; }
    private double getPalvN2(double depthInBar){
        Gas activeGas = diveSettings.getActiveGas();
        return activeGas.getN2Amount() * (depthInBar - diveSettings.getPw());
    }
    private double getPalvHe(double depthInBar) {
        Gas activeGas = diveSettings.getActiveGas();
        return activeGas.getHeAmount() * (depthInBar - diveSettings.getPw());
    }
    private double getPalvN2WithGas(double depthInBar, Gas gas) {
        return gas.getN2Amount() * (depthInBar - diveSettings.getPw());
    }
    private double getPalvHeWithGas(double depthInBar, Gas gas) {
        return gas.getHeAmount() * (depthInBar - diveSettings.getPw());
    }
    private double haldaneTime(double k, double pTissue, double pTissue0, double pAlv){
        double logArg = (pTissue - pAlv) / (pTissue0 - pAlv);
        if (logArg<0) callOut("LogArgument < 0 in HaldaneTime(). pTissue = " + pTissue + " pTissue0 = " + pTissue0 + " pAlv = " + pAlv);
        return -(1 / k) * Math.log((pTissue - pAlv) / (pTissue0 - pAlv));
    }
    private double getPAmbTol(double pTissue, double gf, double a, double b){
        return ((pTissue-gf*a)*b)/(gf-gf*b+b);
    }
    private double getPTol(double pAmb, double a, double b, double gf){
        return a*gf + ((pAmb*(gf-gf*b+b))/b);
    }

    private double getNDL(double depthInBar){
        double ndl = 5940;
        Gas activeGas = diveSettings.getActiveGas();
        double pAlvN2 = getPalvN2WithGas(depthInBar, activeGas);
        double pAlvHe = getPalvHeWithGas(depthInBar, activeGas);
        double maxNdlSeconds = 5940;

        for (int i=0; i<16; i++){
            Tissue tissue = tissueModel.n2Compartments[i];

            if (exceedsTolerance(i, tissue.getPN2(), tissue.getPHe(),
                    diveSettings.getSurfacePressure(), diveSettings.getGf().getHigh())) {
                ndl = -1;
                break;
            }

            double simN2AtMax = simulateInertLoadAfter(tissue.getK(), tissue.getPN2(), pAlvN2, maxNdlSeconds);
            double simHeAtMax = simulateInertLoadAfter(tissue.getHeK(), tissue.getPHe(), pAlvHe, maxNdlSeconds);
            if (!exceedsTolerance(i, simN2AtMax, simHeAtMax,
                    diveSettings.getSurfacePressure(), diveSettings.getGf().getHigh())) {
                continue;
            }

            double low = 0;
            double high = maxNdlSeconds;
            for (int iter = 0; iter < 24; iter++) {
                double mid = (low + high) / 2.0;
                double simN2 = simulateInertLoadAfter(tissue.getK(), tissue.getPN2(), pAlvN2, mid);
                double simHe = simulateInertLoadAfter(tissue.getHeK(), tissue.getPHe(), pAlvHe, mid);
                if (exceedsTolerance(i, simN2, simHe,
                        diveSettings.getSurfacePressure(), diveSettings.getGf().getHigh())) {
                    high = mid;
                } else {
                    low = mid;
                }
            }
            if (high > 0 && high < ndl) ndl = high;
        }
        return ndl;
    }

    private TissueModel calcSafetyStops(TissueModel tissues, int currentStop){
        double stopTime = 0;
        TissueModel instanceTissues = new TissueModel(0, 0);

        for (int i = 0; i < 16; i++) {
            instanceTissues.n2Compartments[i].setPN2(tissues.n2Compartments[i].getPN2());
            instanceTissues.n2Compartments[i].setPHe(tissues.n2Compartments[i].getPHe());
        }

        if(currentStop>0) {
            double stopDepthMeter = barToMeter(stops[currentStop].getStopDepth(), diveSettings);
            Gas bestGas = diveSettings.isMultiGasMode() ?
                    diveSettings.getBestGasForDepth(stopDepthMeter) :
                    diveSettings.getActiveGas();
            if (bestGas == null) bestGas = diveSettings.getActiveGas();

            for (int i = 0; i < 16; i++) {
                Tissue tissue = instanceTissues.n2Compartments[i];
                double pAlvN2 = getPalvN2WithGas(stops[currentStop].getStopDepth(), bestGas);
                double pAlvHe = getPalvHeWithGas(stops[currentStop].getStopDepth(), bestGas);
                double nextStopPressure = stops[currentStop-1].getStopDepth();
                double nextStopGF = diveSettings.getGf().getGF(nextStopPressure);

                if (!exceedsTolerance(i, tissue.getPN2(), tissue.getPHe(), nextStopPressure, nextStopGF)) {
                    continue;
                }

                double maxStopSeconds = 999.0 * 60.0;
                double simN2AtMax = simulateInertLoadAfter(tissue.getK(), tissue.getPN2(), pAlvN2, maxStopSeconds);
                double simHeAtMax = simulateInertLoadAfter(tissue.getHeK(), tissue.getPHe(), pAlvHe, maxStopSeconds);
                if (exceedsTolerance(i, simN2AtMax, simHeAtMax, nextStopPressure, nextStopGF)) {
                    stopTime = Math.max(stopTime, maxStopSeconds);
                    continue;
                }

                double low = 0;
                double high = maxStopSeconds;
                for (int iter = 0; iter < 24; iter++) {
                    double mid = (low + high) / 2.0;
                    double simN2 = simulateInertLoadAfter(tissue.getK(), tissue.getPN2(), pAlvN2, mid);
                    double simHe = simulateInertLoadAfter(tissue.getHeK(), tissue.getPHe(), pAlvHe, mid);
                    if (exceedsTolerance(i, simN2, simHe, nextStopPressure, nextStopGF)) {
                        low = mid;
                    } else {
                        high = mid;
                    }
                }
                if (high > stopTime) stopTime = high;
            }

            if(stopTime > 0 ) {
                stops[currentStop].setActive(true);
                stops[currentStop].setStopTime(stopTime);
                stops[currentStop].setGas(bestGas);
            }else{
                stops[currentStop].setActive(false);
            }

            double pAlvN2 = getPalvN2WithGas(stops[currentStop].getStopDepth(), bestGas);
            double pAlvHe = getPalvHeWithGas(stops[currentStop].getStopDepth(), bestGas);
            setTissueLoadsSchreinerTrimix(instanceTissues, pAlvN2, pAlvHe, 0, 0, stopTime);

            calcSafetyStops(instanceTissues, currentStop-1);
        }
        return instanceTissues;
    }

    private double simulateInertLoadAfter(double k, double initialLoad, double inspiredPressure, double seconds) {
        return inspiredPressure + (initialLoad - inspiredPressure) * Math.exp(-k * seconds);
    }

    private double getMixedAForLoads(int compartmentIndex, double pN2, double pHe) {
        double total = pN2 + pHe;
        Tissue tissue = tissueModel.n2Compartments[compartmentIndex];
        if (total <= 0) return tissue.getA();
        return (tissue.getA() * pN2 + tissue.getHeA() * pHe) / total;
    }

    private double getMixedBForLoads(int compartmentIndex, double pN2, double pHe) {
        double total = pN2 + pHe;
        Tissue tissue = tissueModel.n2Compartments[compartmentIndex];
        if (total <= 0) return tissue.getB();
        return (tissue.getB() * pN2 + tissue.getHeB() * pHe) / total;
    }

    private boolean exceedsTolerance(int compartmentIndex, double pN2, double pHe, double ambientPressure, double gf) {
        double mixedA = getMixedAForLoads(compartmentIndex, pN2, pHe);
        double mixedB = getMixedBForLoads(compartmentIndex, pN2, pHe);
        double pTol = getPTol(ambientPressure, mixedA, mixedB, gf);
        return pN2 + pHe > pTol;
    }

    private double getActualGF(double depthInBar){
        double actualGF=0;
        for ( int i=0; i<16;i++){
            double mixedA = tissueModel.n2Compartments[i].getMixedA();
            double mixedB = tissueModel.n2Compartments[i].getMixedB();
            double pTol = getPTol(depthInBar, mixedA, mixedB, 1);
            double currentLoad = tissueModel.n2Compartments[i].getTotalLoad();
            double actualGFi = (currentLoad-depthInBar)/(pTol-depthInBar);
            if (actualGFi>actualGF) actualGF=actualGFi;
        }
        return actualGF;
    }

    private void setTissueLoadsSchreiner(TissueModel tissues, double pAlv, double R, double time){
        double k;
        double pTissue0;
        for (int i=0; i<16; i++){
            k = tissues.n2Compartments[i].getK();
            pTissue0 = tissues.n2Compartments[i].getLoad();
            tissues.n2Compartments[i].setLoad(pAlv+R*(time-(1/k))-(pAlv-pTissue0-(R/k)) * Math.exp(-k*time));
        }
    }

    private void setTissueLoadsSchreinerTrimix(TissueModel tissues, double pAlvN2, double pAlvHe,
                                               double RN2, double RHe, double time){
        double kN2; double pTissue0N2;
        double kHe; double pTissue0He;
        for (int i=0; i<16; i++){
            kN2 = tissues.n2Compartments[i].getK();
            pTissue0N2 = tissues.n2Compartments[i].getPN2();
            double newPN2 = pAlvN2 + RN2 * (time - (1/kN2)) - (pAlvN2 - pTissue0N2 - (RN2/kN2)) * Math.exp(-kN2 * time);
            tissues.n2Compartments[i].setPN2(newPN2);

            kHe = tissues.n2Compartments[i].getHeK();
            pTissue0He = tissues.n2Compartments[i].getPHe();
            double newPHe = pAlvHe + RHe * (time - (1/kHe)) - (pAlvHe - pTissue0He - (RHe/kHe)) * Math.exp(-kHe * time);
            tissues.n2Compartments[i].setPHe(Math.max(0, newPHe));
        }
    }

    private int getDeepestStop(){
        double deepStop = 0;
        for ( int i=0; i<16; i++){
            double mixedA = tissueModel.n2Compartments[i].getMixedA();
            double mixedB = tissueModel.n2Compartments[i].getMixedB();
            double currentLoad = tissueModel.n2Compartments[i].getTotalLoad();
            double depthi = getPAmbTol(currentLoad, diveSettings.getGf().getLow(), mixedA, mixedB);
            if (depthi > deepStop) deepStop = depthi;
        }

        if (deepStop > diveSettings.getGf().getLowDepth()){
            diveSettings.getGf().setLowDepth(deepStop);
        }

        int n=0;
        do { n++; } while (diveSettings.getGf().getLowDepth() > meterToBar(diveSettings.stops[n], diveSettings));
        return n;
    }

    private void callOut(String string){ System.out.println(string); }

    public DiveDataPoint dive(double pressure, double deltaT){
        int stopN = 0;
        double ndl = 5940;
        Gas activeGas = diveSettings.getActiveGas();
        double pAlvN2Current = getPalvN2WithGas(lastPoint.getDepthInBar(), activeGas);
        double pAlvN2New = getPalvN2WithGas(pressure, activeGas);
        double pAlvHeCurrent = getPalvHeWithGas(lastPoint.getDepthInBar(), activeGas);
        double pAlvHeNew = getPalvHeWithGas(pressure, activeGas);

        double RN2 = (pAlvN2New - pAlvN2Current) / (deltaT);
        double RHe = (pAlvHeNew - pAlvHeCurrent) / (deltaT);

        setTissueLoadsSchreinerTrimix(tissueModel, pAlvN2Current, pAlvHeCurrent, RN2, RHe, deltaT);

        ndl = getNDL(pressure);
        stopN = getDeepestStop();
        calcSafetyStops(tissueModel, stopN);

        double diveTime = lastPoint.getTime() + deltaT;
        int nextStop = getNextStop();

        if (pressure>maxDepth) maxDepth = pressure;
        lastPoint = new DiveDataPoint(diveTime, pressure, ndl, stops[nextStop].getStopDepth(), stops[nextStop].getStopTime(), calcTts(nextStop), 0, getActualGF(lastPoint.getDepthInBar()), maxDepth, 0);

        return lastPoint;
    }

    private int getNextStop(){
        int nextStop = 0;
        for (int i=0; i<40;i++){ if (stops[i].isActive()) nextStop = i; }
        return nextStop;
    }

    private double calcTts(int stopN){
        double tts=0;
        for(int i=1; i<=stopN;i++){ tts += stops[i].getStopTime(); }
        return tts;
    }

    public double getMaxAllowedDepth(){
        Gas activeGas = diveSettings.getActiveGas();
        return activeGas.getMaxPPO2() / activeGas.getOxygenAmount();
    }

    public boolean switchGas(int gasIndex) {
        double currentDepthMeter = barToMeter(lastPoint.getDepthInBar(), diveSettings);
        if (diveSettings.canSwitchToGas(gasIndex, currentDepthMeter)) {
            diveSettings.setActiveGasIndex(gasIndex);
            int stopN = getDeepestStop();
            calcSafetyStops(tissueModel, stopN);
            return true;
        }
        return false;
    }

    public boolean hasBetterGasAvailable() {
        double currentDepthMeter = barToMeter(lastPoint.getDepthInBar(), diveSettings);
        return diveSettings.hasBetterGasAvailable(currentDepthMeter);
    }

    public int getBestGasIndex() {
        double currentDepthMeter = barToMeter(lastPoint.getDepthInBar(), diveSettings);
        return diveSettings.getBestGasIndexForDepth(currentDepthMeter);
    }

    public double barToMeter(double depthInBar, DiveSettings settings){
        return (depthInBar-settings.getSurfacePressure())*settings.getDepthPerBar();
    }
    public double meterToBar(double depthInMeter, DiveSettings settings){
        return settings.getSurfacePressure() + depthInMeter/settings.getDepthPerBar();
    }

    public double getGFLowDepthMeter() {
        return barToMeter(diveSettings.getGf().getLowDepth(), diveSettings);
    }

    public double getCurrentGF(double depthMeter) {
        double depthBar = meterToBar(depthMeter, diveSettings);
        return diveSettings.getGf().getGF(depthBar);
    }

    public double[] getGFInfo(double currentDepthMeter) {
        double[] info = new double[4];
        info[0] = diveSettings.getGf().getLow();
        info[1] = diveSettings.getGf().getHigh();
        info[2] = barToMeter(diveSettings.getGf().getLowDepth(), diveSettings);
        info[3] = getCurrentGF(currentDepthMeter);
        return info;
    }

    public double getCurrentCeilingWithGFLow() {
        double maxAllowedP = diveSettings.getSurfacePressure();
        for (int i = 0; i < 16; i++) {
            double mixedA = tissueModel.n2Compartments[i].getMixedA();
            double mixedB = tissueModel.n2Compartments[i].getMixedB();
            double currentLoad = tissueModel.n2Compartments[i].getTotalLoad();

            double allowedP = getPAmbTol(currentLoad, diveSettings.getGf().getLow(), mixedA, mixedB);
            if (allowedP > maxAllowedP) {
                maxAllowedP = allowedP;
            }
        }
        return maxAllowedP;
    }

    public String generateDecoPlan(double bottomDepthMeter, double bottomTimeMinutes) {
        StringBuilder sb = new StringBuilder();
        sb.append("Stp  Tme  Run  Gas    Qty\n");
        sb.append("─────────────────────────\n");

        double runtime = 0;
        double totalGasQty = 0;
        Gas currentGas = diveSettings.getActiveGas();

        double bottomPressureATA = diveSettings.getSurfacePressure() + bottomDepthMeter / diveSettings.getDepthPerBar();
        double bottomGasQty = diveSettings.getRmv() * bottomTimeMinutes * bottomPressureATA;
        runtime = bottomTimeMinutes;
        totalGasQty += bottomGasQty;

        sb.append(String.format("%3d  bot  %3d  %s  %3d\n",
                (int) bottomDepthMeter, (int) runtime, formatGas(currentGas), (int) Math.round(bottomGasQty)));

        int nextStop = getNextStop();
        if (nextStop > 0) {
            double firstStopDepth = barToMeter(stops[nextStop].getStopDepth(), diveSettings);
            double ascentDepth = bottomDepthMeter - firstStopDepth;
            double ascentTime = ascentDepth / 10.0;
            double avgAscentDepth = (bottomDepthMeter + firstStopDepth) / 2;
            double ascentPressureATA = diveSettings.getSurfacePressure() + avgAscentDepth / diveSettings.getDepthPerBar();
            double ascentGasQty = diveSettings.getRmv() * ascentTime * ascentPressureATA;
            runtime += ascentTime;
            totalGasQty += ascentGasQty;

            Gas ascentGas = diveSettings.isMultiGasMode() ?
                    diveSettings.getBestGasForDepth(firstStopDepth) : currentGas;
            if (ascentGas == null) ascentGas = currentGas;

            sb.append(String.format("%3d  asc  %3d  %s  %3d\n",
                    (int) firstStopDepth, (int) Math.round(runtime), formatGas(ascentGas), (int) Math.round(ascentGasQty)));

            for (int i = nextStop; i >= 1; i--) {
                if (stops[i].isActive()) {
                    double stopDepthMeter = barToMeter(stops[i].getStopDepth(), diveSettings);
                    double stopTimeMinutes = stops[i].getStopTime() / 60.0;

                    Gas stopGas = stops[i].getGas();
                    if (stopGas == null) {
                        stopGas = diveSettings.isMultiGasMode() ?
                                diveSettings.getBestGasForDepth(stopDepthMeter) : currentGas;
                    }
                    if (stopGas == null) stopGas = currentGas;

                    double stopPressureATA = diveSettings.getSurfacePressure() + stopDepthMeter / diveSettings.getDepthPerBar();
                    double stopGasQty = diveSettings.getRmv() * stopTimeMinutes * stopPressureATA;
                    runtime += stopTimeMinutes;
                    totalGasQty += stopGasQty;

                    sb.append(String.format("%3d  %3d  %3d  %s  %3d\n",
                            (int) stopDepthMeter, (int) Math.round(stopTimeMinutes), (int) Math.round(runtime),
                            formatGas(stopGas), (int) Math.round(stopGasQty)));
                }
            }
        }
        sb.append("─────────────────────────\n");
        sb.append(String.format("Total: %d min, %d L\n", (int) Math.round(runtime), (int) Math.round(totalGasQty)));
        return sb.toString();
    }

    private String formatGas(Gas gas) {
        int o2 = (int) Math.round(gas.getOxygenAmount() * 100);
        int he = (int) Math.round(gas.getHeAmount() * 100);
        return String.format("%02d/%02d", o2, he);
    }

    public String getDecoSummary() {
        int nextStop = getNextStop();
        if (nextStop == 0) return "无减压";
        StringBuilder sb = new StringBuilder();
        for (int i = nextStop; i >= 1; i--) {
            if (stops[i].isActive()) {
                double stopDepthMeter = barToMeter(stops[i].getStopDepth(), diveSettings);
                int stopTimeMin = (int) Math.round(stops[i].getStopTime() / 60.0 + 0.5);
                Gas stopGas = stops[i].getGas();
                String gasStr = stopGas != null ? formatGas(stopGas) : "--/--";
                sb.append(String.format("%dm:%dmin(%s) ", (int)stopDepthMeter, stopTimeMin, gasStr));
            }
        }
        return sb.toString().trim();
    }

    public void printDecoDebug() {
        int nextStop = getNextStop();
        double gfLowDepth = barToMeter(diveSettings.getGf().getLowDepth(), diveSettings);

        System.out.println("========== 减压计划调试 ==========");
        System.out.printf("GF 设置: Low=%.0f%% High=%.0f%%\n",
                diveSettings.getGf().getLow() * 100, diveSettings.getGf().getHigh() * 100);
        System.out.printf("首停深度 (LowDepth): %.1fm (%.3f bar)\n", gfLowDepth, diveSettings.getGf().getLowDepth());
        System.out.printf("水面压力 (HighDepth): %.3f bar\n", diveSettings.getSurfacePressure());
        System.out.println("----------------------------------");

        if (nextStop == 0) {
            System.out.println("无减压义务");
            return;
        }

        System.out.printf("%-6s %-10s %-10s %-12s\n", "深度", "时间(秒)", "时间(分)", "GF@该深度");
        for (int i = nextStop; i >= 1; i--) {
            if (stops[i].isActive()) {
                double stopDepthMeter = barToMeter(stops[i].getStopDepth(), diveSettings);
                double stopTimeSec = stops[i].getStopTime();
                double gfAtStop = diveSettings.getGf().getGF(stops[i].getStopDepth());
                double theoreticalGF = calculateTheoreticalGF(stopDepthMeter, gfLowDepth, 0);

                System.out.printf("%4.0fm   %8.1f   %8.1f   %5.1f%% (理论:%.1f%%)\n",
                        stopDepthMeter, stopTimeSec, stopTimeSec / 60.0, gfAtStop * 100, theoreticalGF * 100);
            }
        }
        System.out.println("==================================");
    }

    private double calculateTheoreticalGF(double currentDepth, double firstStopDepth, double finalStopDepth) {
        double gfLow = diveSettings.getGf().getLow();
        double gfHigh = diveSettings.getGf().getHigh();
        double lastStop = diveSettings.getLastStopDepth();

        if (firstStopDepth <= lastStop) return gfHigh;
        if (currentDepth >= firstStopDepth) return gfLow;
        if (currentDepth <= lastStop) return gfHigh;

        double depthRange = firstStopDepth - lastStop;
        double depthFromFirst = firstStopDepth - currentDepth;
        double ratio = depthFromFirst / depthRange;
        return gfLow + (gfHigh - gfLow) * ratio;
    }
}

class DivePlanComparator {
    private double bottomDepthMeter = 40.0;
    private double bottomTimeMinutes = 25;
    private double gas1O2 = 21;
    private double gas2O2 = 0;
    private double gas3O2 = 0;
    private int gfLow = 30;
    private int gfHigh = 70;
    private double bottomPPO2 = 1.4;
    private double decoPPO2 = 1.6;
    private double ascentPPO2 = 1.6;
    private double ascentRate = 10.0;
    private double descentRate = 20.0;
    private int lastStopDepth = 3;
    private double rmv = 14.0;
    private int[][] shearwaterStops = null;
    private int shearwaterTotalDecoTime = 0;

    public DivePlanComparator() { }

    public DivePlanComparator(double depth, double time, int gfL, int gfH, double o2) {
        this.bottomDepthMeter = depth;
        this.bottomTimeMinutes = time;
        this.gfLow = gfL;
        this.gfHigh = gfH;
        this.gas1O2 = o2;
    }

    public void setBottomDepthMeter(double v) { this.bottomDepthMeter = v; }
    public void setBottomTimeMinutes(double v) { this.bottomTimeMinutes = v; }
    public void setGas1O2(double v) { this.gas1O2 = v; }
    public void setGas2O2(double v) { this.gas2O2 = v; }
    public void setGas3O2(double v) { this.gas3O2 = v; }
    public void setGfLow(int v) { this.gfLow = v; }
    public void setGfHigh(int v) { this.gfHigh = v; }
    public void setBottomPPO2(double v) { this.bottomPPO2 = v; }
    public void setDecoPPO2(double v) { this.decoPPO2 = v; }
    public void setAscentPPO2(double v) { this.ascentPPO2 = v; }
    public void setAscentRate(double v) { this.ascentRate = v; }
    public void setDescentRate(double v) { this.descentRate = v; }
    public void setLastStopDepth(int v) { this.lastStopDepth = v; }
    public void setRmv(double v) { this.rmv = v; }
    public void setShearwaterStops(int[][] stops) {
        this.shearwaterStops = stops;
        this.shearwaterTotalDecoTime = 0;
        if (stops != null) {
            for (int[] stop : stops) { this.shearwaterTotalDecoTime += stop[1]; }
        }
    }
    public double getBottomDepthMeter() { return bottomDepthMeter; }
    public double getBottomTimeMinutes() { return bottomTimeMinutes; }
    public int getGfLow() { return gfLow; }
    public int getGfHigh() { return gfHigh; }
    public double getGas1O2() { return gas1O2; }
    public double getGas2O2() { return gas2O2; }
    public double getGas3O2() { return gas3O2; }
    public double getAscentRate() { return ascentRate; }
    public double getDescentRate() { return descentRate; }
    public double getBottomPPO2() { return bottomPPO2; }
    public double getDecoPPO2() { return decoPPO2; }
    public double getAscentPPO2() { return ascentPPO2; }
    public int getLastStopDepth() { return lastStopDepth; }
    public double getRmv() { return rmv; }
    public int getShearwaterTotalDecoTime() { return shearwaterTotalDecoTime; }

    public String runTest() {
        StringBuilder sb = new StringBuilder();
        sb.append("\n╔══════════════════════════════════════════════════════════════════╗\n");
        sb.append("║        Buhlmann ZHL-16 减压计划对比测试                      ║\n");
        sb.append("╚══════════════════════════════════════════════════════════════════╝\n\n");

        DiveSettings settings = createSettings();
        ZHL16 zhl16 = new ZHL16(settings, 0);

        sb.append("╔══════════════════════════════════════════════════════════════════╗\n");
        sb.append("║                        潜水配置                                ║\n");
        sb.append("╚══════════════════════════════════════════════════════════════════╝\n\n");
        sb.append("──────────────────────────────────────────────────────────────────\n");
        sb.append(String.format("  底部深度:     %.1f m\n", bottomDepthMeter));
        sb.append(String.format("  底部时间:     %.0f min\n", bottomTimeMinutes));
        sb.append(String.format("  底部气体:     %.0f%% O2\n", gas1O2));
        sb.append(String.format("  下降速度:     %.0f m/min\n", descentRate));
        sb.append(String.format("  上升速度:     %.0f m/min\n", ascentRate));
        if (gas2O2 > 0) sb.append(String.format("  减压气体1:    %.0f%% O2\n", gas2O2));
        if (gas3O2 > 0) sb.append(String.format("  减压气体2:    %.0f%% O2\n", gas3O2));
        sb.append(String.format("  GF 设置:      %d/%d\n", gfLow, gfHigh));
        sb.append(String.format("  底部 PPO2:   %.1f bar\n", bottomPPO2));
        sb.append(String.format("  上升 PPO2:   %.1f bar\n", ascentPPO2));
        sb.append(String.format("  减压 PPO2:   %.1f bar\n", decoPPO2));
        sb.append(String.format("  最后停留:    %d m\n", lastStopDepth));
        sb.append(String.format("  RMV:         %.1f L/min\n", rmv));
        sb.append("──────────────────────────────────────────────────────────────────\n\n");

        DecoResult result = runFullSimulation(zhl16, settings);

        double descentTimeMin = bottomDepthMeter / descentRate;
        double actualBottomStayMin = bottomTimeMinutes - descentTimeMin;
        double totalRunTime = bottomTimeMinutes + result.totalAscentTimeSec / 60.0;
        double decoTime = result.totalDecoTimeSec / 60.0;

        sb.append("╔══════════════════════════════════════════════════════════════════╗\n");
        sb.append("║                    本算法 ZHL-16 减压计划                        ║\n");
        sb.append("╚══════════════════════════════════════════════════════════════════╝\n\n");
        sb.append("--- 下降阶段 ---\n");
        sb.append(String.format("下降时间: %.1f 分钟 (%.0f m/min)\n\n", descentTimeMin, descentRate));
        sb.append("--- 底部停留 ---\n");
        sb.append(String.format("底部停留: %.1f 分钟\n", actualBottomStayMin));
        sb.append(String.format("底部结束时 TTS: %.1f 分钟\n\n", result.bottomTtsSec / 60.0));
        sb.append("--- 减压停留 ---\n");
        sb.append(String.format("%-12s %15s\n", "深度(m)", "停留时间(分)"));
        sb.append("-----------------------------------\n");

        List<Integer> depths = new ArrayList<>(result.stopTimes.keySet());
        depths.sort((a, b) -> b - a);

        int ourTotalDecoTime = 0;
        for (int depth : depths) {
            double timeMin = result.stopTimes.get(depth) / 60.0;
            sb.append(String.format("%-12d %15.1f\n", depth, timeMin));
            ourTotalDecoTime += (int) Math.round(timeMin);
        }

        sb.append(String.format("\n  总减压时间: %d 分钟\n", ourTotalDecoTime));
        sb.append(String.format("  总潜水时间: %.0f 分钟\n\n", totalRunTime));

        if (shearwaterStops != null && shearwaterStops.length > 0) {
            sb.append("╔══════════════════════════════════════════════════════════════════╗\n");
            sb.append("║                    Shearwater 减压计划                          ║\n");
            sb.append("╚══════════════════════════════════════════════════════════════════╝\n\n");
            sb.append("Stp   Tme   Run\n");
            sb.append("─────────────────────────\n");
            int swTotalDecoTime = 0;
            for (int[] stop : shearwaterStops) {
                swTotalDecoTime += stop[1];
                sb.append(String.format("%3d   %3d   %3d\n", stop[0], stop[1], swTotalDecoTime));
            }
            sb.append("─────────────────────────\n");
            sb.append(String.format("Total Deco: %d min\n\n", shearwaterTotalDecoTime));

            sb.append("╔══════════════════════════════════════════════════════════════════╗\n");
            sb.append("║                        对比表格                                  ║\n");
            sb.append("╚══════════════════════════════════════════════════════════════════╝\n\n");
            sb.append("┌──────────┬────────────┬────────────┬──────────┐\n");
            sb.append("│ 深度(m)  │ 本算法(分) │ Shearwater│  差异    │\n");
            sb.append("├──────────┼────────────┼────────────┼──────────┤\n");

            for (int i = 0; i < shearwaterStops.length; i++) {
                double ourTime = result.getStopTimeMin(shearwaterStops[i][0]);
                int swTime = shearwaterStops[i][1];
                int diff = (int) Math.round(ourTime) - swTime;
                String diffStr = diff > 0 ? "+" + diff : String.valueOf(diff);
                sb.append(String.format("│ %8d │ %10.1f │ %10d │ %8s │\n", shearwaterStops[i][0], ourTime, swTime, diffStr));
            }
            sb.append("├──────────┼────────────┼────────────┼──────────┤\n\n");

            int totalDiff = ourTotalDecoTime - shearwaterTotalDecoTime;
            String totalDiffStr = totalDiff > 0 ? "+" + totalDiff : String.valueOf(totalDiff);
            sb.append("══════════════════════════════════════════════════════════════════\n");
            sb.append(String.format("  总减压时间: 本算法 = %d min, Shearwater = %d min, 差异 = %s min\n",
                    ourTotalDecoTime, shearwaterTotalDecoTime, totalDiffStr));

            if (Math.abs(totalDiff) <= 2) {
                sb.append("  ✓ 结果非常接近！\n");
            } else if (totalDiff > 0) {
                sb.append("  ⚠ 本算法较保守 (时间更长)\n");
            } else {
                sb.append("  ⚠ 本算法较激进 (时间更短)\n");
            }
            sb.append("══════════════════════════════════════════════════════════════════\n");
        }

        sb.append("\n【调试信息 - GF 渐变】\n");
        sb.append("──────────────────────────────────────────────────────────────────\n");
        java.io.ByteArrayOutputStream baos = new java.io.ByteArrayOutputStream();
        java.io.PrintStream ps = new java.io.PrintStream(baos);
        java.io.PrintStream oldOut = System.out;
        System.setOut(ps);
        zhl16.printDecoDebug();
        System.out.flush();
        System.setOut(oldOut);
        sb.append(baos.toString());
        return sb.toString();
    }

    private DecoResult runFullSimulation(ZHL16 zhl16, DiveSettings settings) {
        DecoResult result = new DecoResult();
        double depthBar = meterToBar(bottomDepthMeter, settings);

        double descentTimeMin = bottomDepthMeter / descentRate;
        simulateDescend(zhl16, depthBar, descentTimeMin * 60);

        double bottomStayMin = bottomTimeMinutes - descentTimeMin;
        DiveDataPoint bottomPoint = simulateBottom(zhl16, depthBar, bottomStayMin * 60);
        result.bottomTtsSec = bottomPoint.getTts();

        simulateAscentWithDeco(zhl16, settings, depthBar, ascentRate, result);
        return result;
    }

    private void simulateDescend(ZHL16 zhl16, double targetDepthBar, double timeSec) {
        double step = 1.0;
        double currentDepth = 1.0;
        double increment = (targetDepthBar - 1.0) / (timeSec / step);
        for (double t = 0; t < timeSec; t += step) {
            currentDepth += increment;
            if (currentDepth > targetDepthBar) currentDepth = targetDepthBar;
            zhl16.dive(currentDepth, step);
        }
    }

    private DiveDataPoint simulateBottom(ZHL16 zhl16, double depthBar, double timeSec) {
        double step = 1.0;
        DiveDataPoint point = null;
        for (double t = 0; t < timeSec; t += step) {
            point = zhl16.dive(depthBar, step);
        }
        return point;
    }

    private void simulateAscentWithDeco(ZHL16 zhl16, DiveSettings settings,
                                        double startDepthBar, double ascentRateMpm,
                                        DecoResult result) {
        DiveSettings liveSettings = zhl16.getDiveSettings();
        double currentDepth = startDepthBar;
        double step = 1.0;
        double ascentRateBarPerSec = ascentRateMpm / 60.0 / settings.getDepthPerBar();
        double lastStopBar = settings.getSurfacePressure() + settings.getLastStopDepth() / settings.getDepthPerBar();

        while (currentDepth > lastStopBar - 0.01) {
            switchToBestGasForDepth(liveSettings, barToMeter(currentDepth, liveSettings));
            DiveDataPoint point = zhl16.dive(currentDepth, step);
            result.totalAscentTimeSec += step;
            double stopDepthBar = point.getStopDepth();
            boolean hasStop = stopDepthBar > settings.getSurfacePressure() + 0.01;
            boolean atLastStop = currentDepth <= lastStopBar + 0.05;
            boolean stopAtOrAboveLastStop = stopDepthBar <= lastStopBar + 0.05;

            if (hasStop && currentDepth <= stopDepthBar + 0.05) {
                int stopM = Math.max(settings.getLastStopDepth(), (int) Math.round(barToMeter(stopDepthBar, settings)));
                result.addStopTime(stopM, step);
                result.totalDecoTimeSec += step;
            } else if (hasStop && atLastStop && stopAtOrAboveLastStop) {
                int stopM = settings.getLastStopDepth();
                result.addStopTime(stopM, step);
                result.totalDecoTimeSec += step;
            } else if (!hasStop && atLastStop) {
                break;
            } else {
                currentDepth -= ascentRateBarPerSec;
                if (currentDepth < lastStopBar) {
                    currentDepth = lastStopBar;
                }
            }
        }

        result.totalAscentTimeSec += settings.getLastStopDepth() / (ascentRateMpm / 60.0);
    }

    private void switchToBestGasForDepth(DiveSettings settings, double depthMeter) {
        if (!settings.isMultiGasMode()) return;
        int bestIndex = settings.getBestHigherGasIndexForDepth(depthMeter, settings.getActiveGasIndex());
        if (bestIndex >= 0) settings.setActiveGasIndex(bestIndex);
    }

    private DiveSettings createSettings() {
        DiveSettings settings = new DiveSettings();
        GradientFactors gf = new GradientFactors(gfLow / 100.0, gfHigh / 100.0, settings.getSurfacePressure());
        settings.setGf(gf);
        settings.setBottomPPO2(bottomPPO2);
        settings.setAscentPPO2(ascentPPO2);
        settings.setDecoPPO2(decoPPO2);
        settings.setLastStopDepth(lastStopDepth);
        settings.setRmv(rmv);

        Gas gas = new Gas(gas1O2 / 100.0);
        gas.setMaxPPO2(bottomPPO2);
        settings.setGas(gas);

        if (gas2O2 > 0 || gas3O2 > 0) {
            settings.setMultiGasMode(true);
            if (gas1O2 > 0) {
                Gas gas1 = new Gas(gas1O2 / 100.0);
                gas1.setMaxPPO2(bottomPPO2);
                gas1.setEnabled(true);
                settings.setGasAt(0, gas1);
            }
            if (gas2O2 > 0) {
                Gas gas2 = new Gas(gas2O2 / 100.0);
                gas2.setMaxPPO2(decoPPO2);
                gas2.setEnabled(true);
                settings.setGasAt(1, gas2);
            }
            if (gas3O2 > 0) {
                Gas gas3 = new Gas(gas3O2 / 100.0);
                gas3.setMaxPPO2(decoPPO2);
                gas3.setEnabled(true);
                settings.setGasAt(2, gas3);
            }
            settings.setActiveGasIndex(settings.getBottomGasIndex());
        }
        return settings;
    }

    private double meterToBar(double m, DiveSettings settings) {
        return settings.getSurfacePressure() + m / settings.getDepthPerBar();
    }
    private double barToMeter(double bar, DiveSettings settings) {
        return (bar - settings.getSurfacePressure()) * settings.getDepthPerBar();
    }

    static class DecoResult {
        double totalAscentTimeSec = 0;
        double totalDecoTimeSec = 0;
        double bottomTtsSec = 0;
        Map<Integer, Double> stopTimes = new LinkedHashMap<>();
        void addStopTime(int depthM, double timeSec) { stopTimes.merge(depthM, timeSec, Double::sum); }
        double getStopTimeMin(int depthM) { return stopTimes.getOrDefault(depthM, 0.0) / 60.0; }
    }

    public String runTestSimple() {
        StringBuilder sb = new StringBuilder();
        sb.append("\n======================================================================\n");
        sb.append("              Buhlmann ZHL-16 减压计划计算结果\n");
        sb.append("======================================================================\n\n");

        DiveSettings settings = createSettings();
        ZHL16 zhl16 = new ZHL16(settings, 0);

        sb.append("-------------------------- 潜水配置 --------------------------\n");
        sb.append(String.format("  底部深度:     %.1f 米\n", bottomDepthMeter));
        sb.append(String.format("  底部时间:     %.0f 分钟\n", bottomTimeMinutes));
        sb.append(String.format("  底部气体:     %.0f%% O2\n", gas1O2));
        sb.append(String.format("  下降速度:     %.0f 米/分\n", descentRate));
        sb.append(String.format("  上升速度:     %.0f 米/分\n", ascentRate));
        if (gas2O2 > 0) sb.append(String.format("  减压气体1:    %.0f%% O2\n", gas2O2));
        if (gas3O2 > 0) sb.append(String.format("  减压气体2:    %.0f%% O2\n", gas3O2));
        sb.append(String.format("  GF 设置:      %d/%d\n", gfLow, gfHigh));
        sb.append(String.format("  底部 PPO2:    %.1f bar\n", bottomPPO2));
        sb.append(String.format("  上升 PPO2:    %.1f bar\n", ascentPPO2));
        sb.append(String.format("  减压 PPO2:    %.1f bar\n", decoPPO2));
        sb.append(String.format("  最后停留:     %d 米\n", lastStopDepth));
        sb.append(String.format("  RMV:          %.1f L/分\n", rmv));
        sb.append("----------------------------------------------------------------------\n\n");

        DecoResult result = runFullSimulation(zhl16, settings);

        double descentTimeMin = bottomDepthMeter / descentRate;
        double actualBottomStayMin = bottomTimeMinutes - descentTimeMin;

        sb.append("-------------------------- 下降阶段 --------------------------\n");
        sb.append(String.format("  下降时间: %.1f 分钟 (%.0f 米/分)\n\n", descentTimeMin, descentRate));

        sb.append("-------------------------- 底部停留 --------------------------\n");
        sb.append(String.format("  底部停留: %.1f 分钟\n", actualBottomStayMin));
        sb.append(String.format("  底部结束时 TTS: %.1f 分钟\n\n", result.bottomTtsSec / 60.0));

        sb.append("-------------------------- 减压停留 --------------------------\n");
        sb.append(String.format("  %-12s %15s\n", "深度(米)", "停留时间(分钟)"));
        sb.append("------------------------------------------\n");

        List<Integer> depths = new ArrayList<>(result.stopTimes.keySet());
        depths.sort((a, b) -> b - a);

        int ourTotalDecoTime = 0;
        for (int depth : depths) {
            double timeMin = result.stopTimes.get(depth) / 60.0;
            sb.append(String.format("  %-12d %15.1f\n", depth, timeMin));
            ourTotalDecoTime += (int) Math.round(timeMin);
        }

        double totalRunTime = bottomTimeMinutes + result.totalAscentTimeSec / 60.0;

        sb.append("\n======================================================================\n");
        sb.append(String.format("  总减压时间: %d 分钟\n", ourTotalDecoTime));
        sb.append(String.format("  总潜水时间: %.0f 分钟\n", totalRunTime));
        sb.append("======================================================================\n");

        sb.append("\n【调试信息 - GF 渐变】\n");
        sb.append("----------------------------------------------------------------------\n");
        java.io.ByteArrayOutputStream baos = new java.io.ByteArrayOutputStream();
        java.io.PrintStream ps = new java.io.PrintStream(baos);
        java.io.PrintStream oldOut = System.out;
        System.setOut(ps);
        zhl16.printDecoDebug();
        System.out.flush();
        System.setOut(oldOut);
        sb.append(baos.toString());
        return sb.toString();
    }
}

class DecoPlanEntry {
    public enum EntryType { BOTTOM, ASCENT, DECO_STOP }
    private double depthMeter;
    private double timeMinutes;
    private double runtimeMinutes;
    private Gas gas;
    private double gasQty;
    private EntryType type;

    public DecoPlanEntry(double depthMeter, double timeMinutes, double runtimeMinutes,
                         Gas gas, double gasQty, EntryType type) {
        this.depthMeter = depthMeter;
        this.timeMinutes = timeMinutes;
        this.runtimeMinutes = runtimeMinutes;
        this.gas = gas;
        this.gasQty = gasQty;
        this.type = type;
    }
    public double getDepthMeter() { return depthMeter; }
    public double getTimeMinutes() { return timeMinutes; }
    public double getRuntimeMinutes() { return runtimeMinutes; }
    public Gas getGas() { return gas; }
    public double getGasQty() { return gasQty; }
    public EntryType getEntryType() { return type; }
    public void setTimeMinutes(double timeMinutes) { this.timeMinutes = timeMinutes; }
    public void setGasQty(double gasQty) { this.gasQty = gasQty; }
    public void setRuntimeMinutes(double runtimeMinutes) { this.runtimeMinutes = runtimeMinutes; }

    public String getTimeString() {
        if (type == EntryType.BOTTOM) return "bot";
        if (type == EntryType.ASCENT) return "asc";
        return String.valueOf((int) Math.round(timeMinutes));
    }
    public String getGasString() {
        int o2 = (int) Math.round(gas.getOxygenAmount() * 100);
        int he = (int) Math.round(gas.getHeAmount() * 100);
        return String.format("%02d/%02d", o2, he);
    }
    @Override
    public String toString() {
        return String.format("%3d %4s %3d %s %3d",
                (int) depthMeter, getTimeString(), (int) Math.round(runtimeMinutes),
                getGasString(), (int) Math.round(gasQty));
    }
}

class DecoPlanWizard extends JDialog {
    private DiveSettings settings;
    private ZHL16 zhl16;
    private int depth = 50;
    private int time = 30;
    private int rmv = 14;
    private ArrayList<DecoPlanEntry> decoEntries = new ArrayList<>();
    private Map<String, Integer> gasUsage = new LinkedHashMap<>();
    private int totalRuntime = 0;
    private int totalDecoTime = 0;
    private int cnsPercent = 0;
    private int otuValue = 0;
    private JPanel contentPanel;
    private JPanel headerPanel;
    private JLabel depthValueLabel;
    private JLabel timeValueLabel;
    private JLabel rmvValueLabel;
    private JButton exitButton;
    private JButton nextButton;
    private int currentPage = 0;
    private int decoTablePage = 0;
    private static final int ROWS_PER_PAGE = 8;
    private static final int DEPTH_MIN = 3, DEPTH_MAX = 150;
    private static final int TIME_MIN = 5, TIME_MAX = 180;
    private static final int RMV_MIN = 6, RMV_MAX = 28;
    private static final Color BG_COLOR = new Color(0, 0, 0);
    private static final Color CYAN_COLOR = new Color(0, 255, 255);
    private static final Color WHITE_COLOR = new Color(255, 255, 255);
    private static final Color YELLOW_COLOR = new Color(255, 255, 0);
    private static final Color GREEN_COLOR = new Color(0, 255, 0);

    public DecoPlanWizard(JFrame parent, DiveSettings settings, ZHL16 zhl16) {
        super(parent, "OC Dive Planner", true);
        this.settings = settings;
        this.zhl16 = zhl16;
        this.rmv = (int) settings.getRmv();
        initUI();
        showPage(0);
    }

    private void initUI() {
        setLayout(new BorderLayout());
        setSize(420, 380);
        setLocationRelativeTo(getParent());
        setResizable(false);
        getContentPane().setBackground(BG_COLOR);

        headerPanel = new JPanel(new BorderLayout());
        headerPanel.setBackground(BG_COLOR);
        headerPanel.setBorder(BorderFactory.createEmptyBorder(8, 12, 8, 12));

        JLabel ocLabel = new JLabel("OC");
        ocLabel.setForeground(WHITE_COLOR);
        ocLabel.setFont(new java.awt.Font("Arial", java.awt.Font.BOLD, 14));
        ocLabel.setBorder(BorderFactory.createLineBorder(CYAN_COLOR, 2));
        ocLabel.setHorizontalAlignment(SwingConstants.CENTER);
        ocLabel.setPreferredSize(new Dimension(32, 24));
        headerPanel.add(ocLabel, BorderLayout.WEST);

        JPanel valuesPanel = new JPanel(new GridLayout(2, 3, 10, 2));
        valuesPanel.setBackground(BG_COLOR);
        valuesPanel.setBorder(BorderFactory.createEmptyBorder(0, 20, 0, 0));

        JLabel depthLabel = createLabel("DEPTH", CYAN_COLOR, 11);
        JLabel timeLabel = createLabel("TIME", CYAN_COLOR, 11);
        JLabel rmvLabel = createLabel("RMV", CYAN_COLOR, 11);
        depthLabel.setHorizontalAlignment(SwingConstants.CENTER);
        timeLabel.setHorizontalAlignment(SwingConstants.CENTER);
        rmvLabel.setHorizontalAlignment(SwingConstants.CENTER);
        valuesPanel.add(depthLabel);
        valuesPanel.add(timeLabel);
        valuesPanel.add(rmvLabel);

        depthValueLabel = createLabel("---", WHITE_COLOR, 14);
        timeValueLabel = createLabel("---", WHITE_COLOR, 14);
        rmvValueLabel = createLabel("--", WHITE_COLOR, 14);
        depthValueLabel.setHorizontalAlignment(SwingConstants.CENTER);
        timeValueLabel.setHorizontalAlignment(SwingConstants.CENTER);
        rmvValueLabel.setHorizontalAlignment(SwingConstants.CENTER);
        valuesPanel.add(depthValueLabel);
        valuesPanel.add(timeValueLabel);
        valuesPanel.add(rmvValueLabel);

        headerPanel.add(valuesPanel, BorderLayout.CENTER);

        JLabel trimixInfoLabel = new JLabel("");
        trimixInfoLabel.setForeground(new Color(0, 200, 255));
        trimixInfoLabel.setFont(new java.awt.Font("Arial", java.awt.Font.PLAIN, 10));
        trimixInfoLabel.setBorder(BorderFactory.createEmptyBorder(0, 10, 0, 0));

        if (settings.isTrimixMode()) {
            Gas gas = settings.getActiveGas();
            if (gas.isTrimix()) {
                trimixInfoLabel.setText(gas.getTrimixInfo(settings.getSurfacePressure(),
                        settings.getDepthPerBar(), settings.getMaxEnd()));
            }
        }
        valuesPanel.add(trimixInfoLabel, BorderLayout.EAST);
        add(headerPanel, BorderLayout.NORTH);

        contentPanel = new JPanel();
        contentPanel.setBackground(BG_COLOR);
        contentPanel.setLayout(new BorderLayout());
        add(contentPanel, BorderLayout.CENTER);
 
        JPanel buttonPanel = new JPanel(new BorderLayout());
        buttonPanel.setBackground(BG_COLOR);
        buttonPanel.setBorder(BorderFactory.createCompoundBorder(
                BorderFactory.createMatteBorder(1, 0, 0, 0, CYAN_COLOR),
                BorderFactory.createEmptyBorder(8, 12, 8, 12)));

        exitButton = new JButton("Exit");
        styleButton(exitButton);
        exitButton.addActionListener(e -> dispose());

        nextButton = new JButton("Next >");
        styleButton(nextButton);
        nextButton.addActionListener(e -> nextPage());

        buttonPanel.add(exitButton, BorderLayout.WEST);
        buttonPanel.add(nextButton, BorderLayout.EAST);
        add(buttonPanel, BorderLayout.SOUTH);
    }

    private void styleButton(JButton btn) {
        btn.setForeground(WHITE_COLOR);
        btn.setBackground(BG_COLOR);
        btn.setBorder(BorderFactory.createLineBorder(CYAN_COLOR, 1));
        btn.setFocusPainted(false);
        btn.setPreferredSize(new Dimension(80, 28));
    }

    private void updateHeaderValues() {
        depthValueLabel.setText(String.format("%d", depth));
        timeValueLabel.setText(String.format("%d", time));
        rmvValueLabel.setText(String.format("%d", rmv));
    }

    private void showPage(int page) {
        currentPage = page;
        contentPanel.removeAll();
        switch (page) {
            case 0: showDepthInput(); break;
            case 1: showTimeInput(); break;
            case 2: showRmvInput(); break;
            case 3: showReadyToPlan(); break;
            case 4: showCalculating(); break;
            case 5: showDecoTable(); break;
            case 6: showGasUsage(); break;
            case 7: showSummary(); break;
        }
        contentPanel.revalidate();
        contentPanel.repaint();
    }

    private void nextPage() {
        if (currentPage == 4) return;
        if (currentPage == 5) {
            int totalPages = (int) Math.ceil((double) decoEntries.size() / ROWS_PER_PAGE);
            if (decoTablePage < totalPages - 1) {
                decoTablePage++;
                showPage(5);
                return;
            }
        }
        if (currentPage < 7) {
            if (currentPage == 3) {
                showPage(4);
                SwingWorker<Void, Void> worker = new SwingWorker<Void, Void>() {
                    @Override
                    protected Void doInBackground() {
                        calculateDecoPlan();
                        return null;
                    }
                    @Override
                    protected void done() {
                        decoTablePage = 0;
                        showPage(5);
                    }
                };
                worker.execute();
            } else {
                showPage(currentPage + 1);
            }
        } else {
            dispose();
        }
    }

    private void showDepthInput() {
        JPanel panel = createInputPanel();
        JLabel valueLabel = createLabel(String.format("%d", depth), WHITE_COLOR, 32);
        valueLabel.setBorder(BorderFactory.createMatteBorder(0, 0, 3, 0, YELLOW_COLOR));
        valueLabel.setHorizontalAlignment(SwingConstants.CENTER);

        JLabel titleLabel = createLabel("Enter Bottom Depth", WHITE_COLOR, 16);
        titleLabel.setHorizontalAlignment(SwingConstants.CENTER);
        JLabel unitLabel = createLabel("in meters", WHITE_COLOR, 12);
        unitLabel.setHorizontalAlignment(SwingConstants.CENTER);

        JLabel minLabel = createLabel("MIN: " + DEPTH_MIN, WHITE_COLOR, 11);
        JLabel maxLabel = createLabel("MAX: " + DEPTH_MAX, WHITE_COLOR, 11);
        minLabel.setHorizontalAlignment(SwingConstants.CENTER);
        maxLabel.setHorizontalAlignment(SwingConstants.CENTER);

        final JSpinner spinner = new JSpinner(new SpinnerNumberModel(depth, DEPTH_MIN, DEPTH_MAX, 1));
        spinner.setFont(new java.awt.Font("Monospaced", java.awt.Font.BOLD, 16));
        spinner.setPreferredSize(new Dimension(100, 30));
        spinner.addChangeListener(e -> {
            depth = (Integer) spinner.getValue();
            valueLabel.setText(String.format("%d", depth));
        });

        JPanel spinnerPanel = new JPanel(new FlowLayout(FlowLayout.CENTER));
        spinnerPanel.setBackground(BG_COLOR);
        spinnerPanel.add(spinner);

        panel.add(Box.createVerticalStrut(20));
        panel.add(valueLabel);
        panel.add(Box.createVerticalStrut(25));
        panel.add(titleLabel);
        panel.add(Box.createVerticalStrut(5));
        panel.add(unitLabel);
        panel.add(Box.createVerticalStrut(15));
        panel.add(minLabel);
        panel.add(maxLabel);
        panel.add(Box.createVerticalStrut(20));
        panel.add(spinnerPanel);

        contentPanel.add(panel, BorderLayout.CENTER);
        nextButton.setText("Next >");
        nextButton.setEnabled(true);

        depthValueLabel.setText("---");
        timeValueLabel.setText("---");
        rmvValueLabel.setText("--");
    }

    private void showTimeInput() {
        JPanel panel = createInputPanel();
        JLabel valueLabel = createLabel(String.format("%d", time), WHITE_COLOR, 32);
        valueLabel.setBorder(BorderFactory.createMatteBorder(0, 0, 3, 0, YELLOW_COLOR));
        valueLabel.setHorizontalAlignment(SwingConstants.CENTER);

        JLabel titleLabel = createLabel("Enter Bottom Time", WHITE_COLOR, 16);
        titleLabel.setHorizontalAlignment(SwingConstants.CENTER);
        JLabel unitLabel = createLabel("in minutes", WHITE_COLOR, 12);
        unitLabel.setHorizontalAlignment(SwingConstants.CENTER);

        JLabel minLabel = createLabel("MIN: " + TIME_MIN, WHITE_COLOR, 11);
        JLabel maxLabel = createLabel("MAX: " + TIME_MAX, WHITE_COLOR, 11);
        minLabel.setHorizontalAlignment(SwingConstants.CENTER);
        maxLabel.setHorizontalAlignment(SwingConstants.CENTER);

        final JSpinner spinner = new JSpinner(new SpinnerNumberModel(time, TIME_MIN, TIME_MAX, 1));
        spinner.setFont(new java.awt.Font("Monospaced", java.awt.Font.BOLD, 16));
        spinner.setPreferredSize(new Dimension(100, 30));
        spinner.addChangeListener(e -> {
            time = (Integer) spinner.getValue();
            valueLabel.setText(String.format("%d", time));
        });

        JPanel spinnerPanel = new JPanel(new FlowLayout(FlowLayout.CENTER));
        spinnerPanel.setBackground(BG_COLOR);
        spinnerPanel.add(spinner);

        panel.add(Box.createVerticalStrut(20));
        panel.add(valueLabel);
        panel.add(Box.createVerticalStrut(25));
        panel.add(titleLabel);
        panel.add(Box.createVerticalStrut(5));
        panel.add(unitLabel);
        panel.add(Box.createVerticalStrut(15));
        panel.add(minLabel);
        panel.add(maxLabel);
        panel.add(Box.createVerticalStrut(20));
        panel.add(spinnerPanel);

        contentPanel.add(panel, BorderLayout.CENTER);
        depthValueLabel.setText(String.format("%d", depth));
    }

    private void showRmvInput() {
        JPanel panel = createInputPanel();
        JLabel valueLabel = createLabel(String.format("%d", rmv), WHITE_COLOR, 32);
        valueLabel.setBorder(BorderFactory.createMatteBorder(0, 0, 3, 0, YELLOW_COLOR));
        valueLabel.setHorizontalAlignment(SwingConstants.CENTER);

        JLabel titleLabel = createLabel("Enter RMV", WHITE_COLOR, 16);
        titleLabel.setHorizontalAlignment(SwingConstants.CENTER);
        JLabel unitLabel = createLabel("in Liters/min", WHITE_COLOR, 12);
        unitLabel.setHorizontalAlignment(SwingConstants.CENTER);

        JLabel minLabel = createLabel("MIN: " + RMV_MIN, WHITE_COLOR, 11);
        JLabel maxLabel = createLabel("MAX: " + RMV_MAX, WHITE_COLOR, 11);
        minLabel.setHorizontalAlignment(SwingConstants.CENTER);
        maxLabel.setHorizontalAlignment(SwingConstants.CENTER);

        final JSpinner spinner = new JSpinner(new SpinnerNumberModel(rmv, RMV_MIN, RMV_MAX, 1));
        spinner.setFont(new java.awt.Font("Monospaced", java.awt.Font.BOLD, 16));
        spinner.setPreferredSize(new Dimension(100, 30));
        spinner.addChangeListener(e -> {
            rmv = (Integer) spinner.getValue();
            valueLabel.setText(String.format("%d", rmv));
        });

        JPanel spinnerPanel = new JPanel(new FlowLayout(FlowLayout.CENTER));
        spinnerPanel.setBackground(BG_COLOR);
        spinnerPanel.add(spinner);

        panel.add(Box.createVerticalStrut(20));
        panel.add(valueLabel);
        panel.add(Box.createVerticalStrut(25));
        panel.add(titleLabel);
        panel.add(Box.createVerticalStrut(5));
        panel.add(unitLabel);
        panel.add(Box.createVerticalStrut(15));
        panel.add(minLabel);
        panel.add(maxLabel);
        panel.add(Box.createVerticalStrut(20));
        panel.add(spinnerPanel);

        contentPanel.add(panel, BorderLayout.CENTER);
        timeValueLabel.setText(String.format("%d", time));
    }

    private void showReadyToPlan() {
        JPanel panel = createInputPanel();
        updateHeaderValues();
        JLabel titleLabel = createLabel("Ready to Plan Dive", WHITE_COLOR, 18);
        titleLabel.setHorizontalAlignment(SwingConstants.CENTER);

        int gfLow = (int)(settings.getGf().getLow() * 100);
        int gfHigh = (int)(settings.getGf().getHigh() * 100);

        JLabel gfLabel = createLabel(String.format("GF:           %d/%d", gfLow, gfHigh), WHITE_COLOR, 14);
        JLabel lastStopLabel = createLabel(String.format("Last Stop:    %dm", settings.getLastStopDepth()), WHITE_COLOR, 14);
        JLabel cnsLabel = createLabel("Start CNS:    0%", WHITE_COLOR, 14);

        panel.add(Box.createVerticalStrut(30));
        panel.add(titleLabel);
        panel.add(Box.createVerticalStrut(40));
        panel.add(gfLabel);
        panel.add(Box.createVerticalStrut(12));
        panel.add(lastStopLabel);
        panel.add(Box.createVerticalStrut(12));
        panel.add(cnsLabel);

        contentPanel.add(panel, BorderLayout.CENTER);
        nextButton.setText("Plan >");
    }

    private void showCalculating() {
        JPanel panel = new JPanel(new BorderLayout());
        panel.setBackground(BG_COLOR);
        JLabel calcLabel = createLabel("Calculating...", CYAN_COLOR, 22);
        calcLabel.setHorizontalAlignment(SwingConstants.CENTER);
        panel.add(calcLabel, BorderLayout.CENTER);
        contentPanel.add(panel, BorderLayout.CENTER);
        nextButton.setEnabled(false);
    }

    private void showDecoTable() {
        JPanel panel = new JPanel(new BorderLayout());
        panel.setBackground(BG_COLOR);
        panel.setBorder(BorderFactory.createEmptyBorder(5, 15, 5, 15));

        JPanel headerRow = new JPanel(new GridLayout(1, 5));
        headerRow.setBackground(BG_COLOR);
        headerRow.add(createTableHeader("Stp"));
        headerRow.add(createTableHeader("Tme"));
        headerRow.add(createTableHeader("Run"));
        headerRow.add(createTableHeader("Gas"));
        headerRow.add(createTableHeader("Qty"));

        JPanel dataPanel = new JPanel();
        dataPanel.setBackground(BG_COLOR);
        dataPanel.setLayout(new BoxLayout(dataPanel, BoxLayout.Y_AXIS));

        int startIdx = decoTablePage * ROWS_PER_PAGE;
        int endIdx = Math.min(startIdx + ROWS_PER_PAGE, decoEntries.size());

        for (int i = startIdx; i < endIdx; i++) {
            DecoPlanEntry entry = decoEntries.get(i);
            JPanel rowPanel = new JPanel(new GridLayout(1, 5));
            rowPanel.setBackground(BG_COLOR);
            rowPanel.setMaximumSize(new Dimension(Integer.MAX_VALUE, 22));

            rowPanel.add(createTableCell(String.valueOf((int) entry.getDepthMeter())));
            rowPanel.add(createTableCell(entry.getTimeString()));
            rowPanel.add(createTableCell(String.valueOf((int) entry.getRuntimeMinutes())));
            rowPanel.add(createTableCell(entry.getGasString()));
            rowPanel.add(createTableCell(String.valueOf((int) entry.getGasQty())));

            dataPanel.add(rowPanel);
        }

        int totalPages = (int) Math.ceil((double) decoEntries.size() / ROWS_PER_PAGE);
        JLabel pageLabel = createLabel(String.format("Page %d/%d", decoTablePage + 1, totalPages), CYAN_COLOR, 10);
        pageLabel.setHorizontalAlignment(SwingConstants.CENTER);

        panel.add(headerRow, BorderLayout.NORTH);
        panel.add(dataPanel, BorderLayout.CENTER);
        panel.add(pageLabel, BorderLayout.SOUTH);

        contentPanel.add(panel, BorderLayout.CENTER);
        nextButton.setEnabled(true);
        nextButton.setText(decoTablePage < totalPages - 1 ? "More >" : "Next >");
    }

    private void showGasUsage() {
        JPanel panel = createInputPanel();
        JLabel titleLabel = createLabel("Gas Usage", WHITE_COLOR, 18);
        titleLabel.setHorizontalAlignment(SwingConstants.CENTER);
        JLabel unitLabel = createLabel("in Liters", WHITE_COLOR, 12);
        unitLabel.setHorizontalAlignment(SwingConstants.CENTER);

        panel.add(Box.createVerticalStrut(25));
        panel.add(titleLabel);
        panel.add(Box.createVerticalStrut(5));
        panel.add(unitLabel);
        panel.add(Box.createVerticalStrut(30));

        for (Map.Entry<String, Integer> entry : gasUsage.entrySet()) {
            JLabel gasLabel = createLabel(String.format("%s:     %d", entry.getKey(), entry.getValue()), WHITE_COLOR, 16);
            gasLabel.setHorizontalAlignment(SwingConstants.CENTER);
            panel.add(gasLabel);
            panel.add(Box.createVerticalStrut(10));
        }

        contentPanel.add(panel, BorderLayout.CENTER);
        nextButton.setText("Next >");
    }

    private void showSummary() {
        JPanel panel = createInputPanel();
        JLabel titleLabel = createLabel("OC Summary", WHITE_COLOR, 18);
        titleLabel.setHorizontalAlignment(SwingConstants.CENTER);

        JLabel runLabel = createLabel(String.format("Run:     %d min", totalRuntime), WHITE_COLOR, 16);
        JLabel decoLabel = createLabel(String.format("Deco:    %d min", totalDecoTime), WHITE_COLOR, 16);
        JLabel cnsLabel = createLabel(String.format("CNS:     %d %%", cnsPercent), WHITE_COLOR, 16);
        JLabel otuLabel = createLabel(String.format("OTU:     %d", otuValue), WHITE_COLOR, 16);

        panel.add(Box.createVerticalStrut(30));
        panel.add(titleLabel);
        panel.add(Box.createVerticalStrut(40));
        panel.add(runLabel);
        panel.add(Box.createVerticalStrut(15));
        panel.add(decoLabel);
        panel.add(Box.createVerticalStrut(15));
        panel.add(cnsLabel);
        panel.add(Box.createVerticalStrut(15));
        panel.add(otuLabel);

        contentPanel.add(panel, BorderLayout.CENTER);
        nextButton.setText("Done");
    }

    private static final double DESCENT_RATE = 18.0;
    private static final double ASCENT_RATE = 10.0;

    private void calculateDecoPlan() {
        decoEntries.clear();
        gasUsage.clear();
        settings.setRmv(rmv);

        DivePlanComparator comparator = new DivePlanComparator();
        comparator.setBottomDepthMeter(depth);
        comparator.setBottomTimeMinutes(time);
        Gas bottomGas = settings.isMultiGasMode() ? settings.getGases()[settings.getBottomGasIndex()] : settings.getActiveGas();
        comparator.setGas1O2(bottomGas.getOxygenAmount() * 100);
        comparator.setGfLow((int)(settings.getGf().getLow() * 100));
        comparator.setGfHigh((int)(settings.getGf().getHigh() * 100));
        comparator.setDescentRate(DESCENT_RATE);
        comparator.setAscentRate(ASCENT_RATE);
        comparator.setBottomPPO2(settings.getBottomPPO2());
        comparator.setAscentPPO2(settings.getAscentPPO2());
        comparator.setDecoPPO2(settings.getDecoPPO2());
        comparator.setLastStopDepth(settings.getLastStopDepth());
        comparator.setRmv(rmv);

        DiveSettings simSettings = createSettingsFromComparator(comparator);
        ZHL16 tempZhl = new ZHL16(simSettings, 0);

        DecoResult result = runSimulation(tempZhl, simSettings, comparator);

        convertToDecoEntries(comparator, result, simSettings);
        totalRuntime = (int)(time + result.totalAscentTimeSec / 60.0);
        OxygenExposure exposure = calculateOxygenExposure(comparator, simSettings);
        cnsPercent = (int) Math.round(exposure.cns);
        otuValue = (int) Math.round(exposure.otu);

        printDecoPlan();
    }

    private DiveSettings createSettingsFromComparator(DivePlanComparator comparator) {
        DiveSettings simSettings = new DiveSettings(
                settings.getSurfacePressure(),
                settings.getWaterDensity().getMetersPerBar(),
                new GradientFactors(comparator.getGfLow() / 100.0, comparator.getGfHigh() / 100.0, settings.getSurfacePressure()),
                settings.getGas(),
                settings.getMaxPP02()
        );
        simSettings.setWaterDensity(settings.getWaterDensity());
        GradientFactors gf = new GradientFactors(
                comparator.getGfLow() / 100.0,
                comparator.getGfHigh() / 100.0,
                settings.getSurfacePressure()
        );
        simSettings.setGf(gf);
        simSettings.setBottomPPO2(comparator.getBottomPPO2());
        simSettings.setAscentPPO2(comparator.getAscentPPO2());
        simSettings.setDecoPPO2(comparator.getDecoPPO2());
        simSettings.setLastStopDepth(comparator.getLastStopDepth());
        simSettings.setRmv(comparator.getRmv());

        Gas gas;
        if (settings.isTrimixMode()) {
            Gas trimixBottomGas = settings.isMultiGasMode() ? settings.getGases()[settings.getBottomGasIndex()] : settings.getActiveGas();
            gas = new Gas(trimixBottomGas.getOxygenAmount(), trimixBottomGas.getHeAmount());
            simSettings.setTrimixMode(true);
            simSettings.setTrimixO2(settings.getTrimixO2());
            simSettings.setTrimixHe(settings.getTrimixHe());
            simSettings.setMaxEnd(settings.getMaxEnd());
        } else {
            gas = new Gas(comparator.getGas1O2() / 100.0);
        }
        gas.setMaxPPO2(comparator.getBottomPPO2());
        simSettings.setGas(gas);

        if (settings.isMultiGasMode()) {
            simSettings.setMultiGasMode(true);
            for (int i = 0; i < 3; i++) {
                Gas g = settings.getGases()[i];
                Gas simGas = new Gas(g.getOxygenAmount(), g.getHeAmount());
                simGas.setMaxPPO2(g.getMaxPPO2());
                simGas.setEnabled(g.isEnabled());
                simGas.setName(g.getName());
                simSettings.setGasAt(i, simGas);
            }
            simSettings.setActiveGasIndex(simSettings.getBottomGasIndex());
        }
        return simSettings;
    }

    private DecoResult runSimulation(ZHL16 zhl16, DiveSettings simSettings, DivePlanComparator comparator) {
        DecoResult result = new DecoResult();
        double depthBar = simSettings.getSurfacePressure() + comparator.getBottomDepthMeter() / simSettings.getDepthPerBar();
        double descentRate = comparator.getDescentRate();
        double ascentRate = comparator.getAscentRate();

        double descentTimeMin = comparator.getBottomDepthMeter() / descentRate;
        simulateDescend(zhl16, depthBar, descentTimeMin * 60);

        double bottomStayMin = comparator.getBottomTimeMinutes() - descentTimeMin;
        simulateBottom(zhl16, depthBar, bottomStayMin * 60);

        simulateAscentWithDeco(zhl16, simSettings, depthBar, ascentRate, result);
        return result;
    }

    private void simulateDescend(ZHL16 zhl16, double targetDepthBar, double timeSec) {
        double step = 1.0;
        double currentDepth = 1.0;
        double increment = (targetDepthBar - 1.0) / (timeSec / step);
        for (double t = 0; t < timeSec; t += step) {
            currentDepth += increment;
            if (currentDepth > targetDepthBar) currentDepth = targetDepthBar;
            zhl16.dive(currentDepth, step);
        }
    }

    private void simulateBottom(ZHL16 zhl16, double depthBar, double timeSec) {
        double step = 1.0;
        for (double t = 0; t < timeSec; t += step) { zhl16.dive(depthBar, step); }
    }

    private void simulateAscentWithDeco(ZHL16 zhl16, DiveSettings simSettings,
                                        double startDepthBar, double ascentRateMpm,
                                        DecoResult result) {
        DiveSettings liveSettings = zhl16.getDiveSettings();
        double currentDepth = startDepthBar;
        double step = 1.0;
        double ascentRateBarPerSec = ascentRateMpm / 60.0 / simSettings.getDepthPerBar();
        double lastStopBar = simSettings.getSurfacePressure() + simSettings.getLastStopDepth() / simSettings.getDepthPerBar();

        while (currentDepth > lastStopBar - 0.01) {
            switchToBestGasForDepth(liveSettings, barToMeter(currentDepth, liveSettings));
            DiveDataPoint point = zhl16.dive(currentDepth, step);
            result.totalAscentTimeSec += step;
            double stopDepthBar = point.getStopDepth();
            boolean hasStop = stopDepthBar > simSettings.getSurfacePressure() + 0.01;
            boolean atLastStop = currentDepth <= lastStopBar + 0.05;
            boolean stopAtOrAboveLastStop = stopDepthBar <= lastStopBar + 0.05;

            if (hasStop && currentDepth <= stopDepthBar + 0.05) {
                int stopM = Math.max(simSettings.getLastStopDepth(), (int) Math.round(barToMeter(stopDepthBar, simSettings)));
                result.addStopTime(stopM, step);
                result.totalDecoTimeSec += step;
            } else if (hasStop && atLastStop && stopAtOrAboveLastStop) {
                int stopM = simSettings.getLastStopDepth();
                result.addStopTime(stopM, step);
                result.totalDecoTimeSec += step;
            } else if (!hasStop && atLastStop) {
                break;
            } else {
                currentDepth -= ascentRateBarPerSec;
                if (currentDepth < lastStopBar) {
                    currentDepth = lastStopBar;
                }
            }
        }

        result.totalAscentTimeSec += simSettings.getLastStopDepth() / (ascentRateMpm / 60.0);
    }

    private void switchToBestGasForDepth(DiveSettings simSettings, double depthMeter) {
        if (!simSettings.isMultiGasMode()) return;
        int bestIndex = simSettings.getBestHigherGasIndexForDepth(depthMeter, simSettings.getActiveGasIndex());
        if (bestIndex >= 0) simSettings.setActiveGasIndex(bestIndex);
    }

    private double barToMeter(double bar, DiveSettings simSettings) {
        return (bar - simSettings.getSurfacePressure()) * simSettings.getDepthPerBar();
    }

    private void convertToDecoEntries(DivePlanComparator comparator, DecoResult result, DiveSettings simSettings) {
        decoEntries.clear();
        gasUsage.clear();

        double currentDepth = comparator.getBottomDepthMeter();
        Gas currentGas = simSettings.getActiveGas();

        double pSurf = simSettings.getSurfacePressure();
        double dPerBar = simSettings.getDepthPerBar();

        double descentTimeMin = currentDepth / comparator.getDescentRate();
        double avgDescentPressure = pSurf + (currentDepth / 2.0) / dPerBar;
        double descentQty = rmv * descentTimeMin * avgDescentPressure;

        double bottomStayMin = Math.max(0, comparator.getBottomTimeMinutes() - descentTimeMin);
        double bottomPressure = pSurf + currentDepth / dPerBar;
        double bottomStayQty = rmv * bottomStayMin * bottomPressure;

        double totalBottomQty = descentQty + bottomStayQty;
        double[] runtime = new double[] { comparator.getBottomTimeMinutes() };

        decoEntries.add(new DecoPlanEntry(
                currentDepth, (int) Math.round(bottomStayMin), runtime[0],
                currentGas, (int) totalBottomQty, DecoPlanEntry.EntryType.BOTTOM
        ));
        addGasUsage(currentGas, totalBottomQty);

        java.util.List<Integer> depths = new java.util.ArrayList<>(result.stopTimes.keySet());
        int finalStopLimit = comparator.getLastStopDepth();
        depths.removeIf(d -> d < finalStopLimit);
        depths.sort((a, b) -> b - a);

        int totalDeco = 0;

        if (!depths.isEmpty()) {
            int firstStopDepth = depths.get(0);
            currentGas = addAscentEntries(currentDepth, firstStopDepth, currentGas, simSettings,
                    comparator.getAscentRate(), pSurf, dPerBar, runtime, true);
            currentDepth = firstStopDepth;

            for (int i = 0; i < depths.size(); i++) {
                int stopDepth = depths.get(i);
                if (currentDepth > stopDepth + 0.01) {
                    currentGas = addAscentEntries(currentDepth, stopDepth, currentGas, simSettings,
                            comparator.getAscentRate(), pSurf, dPerBar, runtime, false);
                    currentDepth = stopDepth;
                }

                currentGas = switchToBestAvailableGasForDepth(simSettings, stopDepth, currentGas);
                double timeSec = result.stopTimes.get(stopDepth);
                int timeMin = (int) Math.round(timeSec / 60.0);

                if (timeMin > 0) {
                    double stopPressure = pSurf + stopDepth / dPerBar;
                    double stopQty = rmv * timeMin * stopPressure;

                    runtime[0] += timeMin;
                    totalDeco += timeMin;

                    decoEntries.add(new DecoPlanEntry(
                            stopDepth, timeMin, runtime[0], currentGas, (int) stopQty, DecoPlanEntry.EntryType.DECO_STOP
                    ));
                    addGasUsage(currentGas, stopQty);
                }

                int nextDepth = (i < depths.size() - 1) ? depths.get(i + 1) : 0;
                currentGas = addAscentEntries(stopDepth, nextDepth, currentGas, simSettings,
                        comparator.getAscentRate(), pSurf, dPerBar, runtime, false);
                currentDepth = nextDepth;
            }
        } else {
            addAscentEntries(currentDepth, 0, currentGas, simSettings,
                    comparator.getAscentRate(), pSurf, dPerBar, runtime, true);
        }
        totalDecoTime = totalDeco;
    }

    private Gas addAscentEntries(double fromDepth, double toDepth, Gas currentGas, DiveSettings simSettings,
                                 double ascentRate, double pSurf, double dPerBar, double[] runtime,
                                 boolean alwaysShowTarget) {
        double segmentStart = fromDepth;
        Gas segmentGas = switchToBestAvailableGasForDepth(simSettings, segmentStart, currentGas);
        boolean wroteAscentEntry = false;

        while (segmentStart > toDepth + 0.01) {
            int switchIndex = findNextSwitchGasIndex(segmentStart, toDepth, segmentGas, simSettings);
            double segmentEnd = toDepth;
            boolean switchAtEnd = false;

            if (switchIndex >= 0) {
                segmentEnd = Math.max(toDepth, simSettings.calculateMODForGas(simSettings.getGases()[switchIndex]));
                if (segmentEnd >= segmentStart - 0.01) {
                    simSettings.setActiveGasIndex(switchIndex);
                    segmentGas = simSettings.getGases()[switchIndex];
                    continue;
                }
                switchAtEnd = true;
            }

            double ascentTime = (segmentStart - segmentEnd) / ascentRate;
            double avgAscentPressure = pSurf + ((segmentStart + segmentEnd) / 2.0) / dPerBar;
            double ascentQty = rmv * ascentTime * avgAscentPressure;
            runtime[0] += ascentTime;
            addGasUsage(segmentGas, ascentQty);

            if (alwaysShowTarget || switchAtEnd || wroteAscentEntry) {
                decoEntries.add(new DecoPlanEntry(
                        segmentEnd, Math.ceil(ascentTime), runtime[0],
                        segmentGas, (int) ascentQty, DecoPlanEntry.EntryType.ASCENT
                ));
                wroteAscentEntry = true;
            }

            segmentStart = segmentEnd;
            if (switchAtEnd) {
                simSettings.setActiveGasIndex(switchIndex);
                segmentGas = simSettings.getGases()[switchIndex];
                segmentGas = switchToBestAvailableGasForDepth(simSettings, segmentStart, segmentGas);
            } else {
                break;
            }
        }

        return segmentGas;
    }

    private Gas switchToBestAvailableGasForDepth(DiveSettings simSettings, double depthMeter, Gas currentGas) {
        if (!simSettings.isMultiGasMode()) return currentGas;
        int currentIndex = simSettings.getGasIndex(currentGas);
        if (currentIndex < 0) currentIndex = simSettings.getActiveGasIndex();
        int bestIndex = simSettings.getBestHigherGasIndexForDepth(depthMeter, currentIndex);
        if (bestIndex >= 0) {
            simSettings.setActiveGasIndex(bestIndex);
            return simSettings.getGases()[bestIndex];
        }
        return currentGas;
    }

    private int findNextSwitchGasIndex(double currentDepth, double targetDepth, Gas currentGas, DiveSettings simSettings) {
        if (!simSettings.isMultiGasMode()) return -1;
        double currentO2 = currentGas.getOxygenAmount();
        int bestIndex = -1;
        double bestSwitchDepth = -1;
        double bestO2 = currentO2;

        Gas[] gases = simSettings.getGases();
        for (int i = 0; i < gases.length; i++) {
            Gas gas = gases[i];
            if (gas == null || !gas.isEnabled() || gas.getOxygenAmount() <= currentO2 + 0.0001) continue;

            double mod = simSettings.calculateMODForGas(gas);
            if (mod < currentDepth - 0.01 && mod >= targetDepth - 0.01) {
                if (mod > bestSwitchDepth + 0.01 ||
                        (Math.abs(mod - bestSwitchDepth) <= 0.01 && gas.getOxygenAmount() > bestO2)) {
                    bestSwitchDepth = mod;
                    bestO2 = gas.getOxygenAmount();
                    bestIndex = i;
                }
            }
        }
        return bestIndex;
    }

    private void addGasUsage(Gas gas, double qty) {
        int roundedQty = (int) Math.round(qty);
        String key = formatGasKey(gas);
        gasUsage.put(key, gasUsage.getOrDefault(key, 0) + roundedQty);
    }

    private String formatGasKey(Gas gas) {
        int o2 = (int) Math.round(gas.getOxygenAmount() * 100);
        int he = (int) Math.round(gas.getHeAmount() * 100);
        return String.format("%02d/%02d", o2, he);
    }

    private OxygenExposure calculateOxygenExposure(DivePlanComparator comparator, DiveSettings simSettings) {
        OxygenExposure exposure = new OxygenExposure();
        if (decoEntries.isEmpty()) return exposure;

        double currentDepth = 0;
        DecoPlanEntry bottomEntry = decoEntries.get(0);
        Gas currentGas = bottomEntry.getGas();

        double descentTimeMin = comparator.getBottomDepthMeter() / comparator.getDescentRate();
        addOxygenExposure(exposure, currentGas, comparator.getBottomDepthMeter() / 2.0, descentTimeMin, simSettings);

        double bottomStayMin = Math.max(0, comparator.getBottomTimeMinutes() - descentTimeMin);
        addOxygenExposure(exposure, currentGas, comparator.getBottomDepthMeter(), bottomStayMin, simSettings);
        currentDepth = comparator.getBottomDepthMeter();

        for (int i = 1; i < decoEntries.size(); i++) {
            DecoPlanEntry entry = decoEntries.get(i);
            Gas gas = entry.getGas();
            double entryDepth = entry.getDepthMeter();

            if (entry.getEntryType() == DecoPlanEntry.EntryType.ASCENT) {
                double avgDepth = (currentDepth + entryDepth) / 2.0;
                addOxygenExposure(exposure, gas, avgDepth, entry.getTimeMinutes(), simSettings);
                currentDepth = entryDepth;
            } else if (entry.getEntryType() == DecoPlanEntry.EntryType.DECO_STOP) {
                addOxygenExposure(exposure, gas, entryDepth, entry.getTimeMinutes(), simSettings);

                double nextDepth = 0;
                for (int j = i + 1; j < decoEntries.size(); j++) {
                    DecoPlanEntry next = decoEntries.get(j);
                    if (next.getEntryType() == DecoPlanEntry.EntryType.DECO_STOP) {
                        nextDepth = next.getDepthMeter();
                        break;
                    }
                }
                double ascentTime = Math.max(0, (entryDepth - nextDepth) / ASCENT_RATE);
                addOxygenExposure(exposure, gas, (entryDepth + nextDepth) / 2.0, ascentTime, simSettings);
                currentDepth = nextDepth;
            }
        }

        exposure.cns = Math.min(999.0, exposure.cns);
        exposure.otu = Math.min(999.0, exposure.otu);
        return exposure;
    }

    private void addOxygenExposure(OxygenExposure exposure, Gas gas, double depthMeter,
                                   double timeMinutes, DiveSettings simSettings) {
        if (gas == null || timeMinutes <= 0) return;
        double ambientPressure = simSettings.getSurfacePressure() + depthMeter / simSettings.getDepthPerBar();
        double ppo2 = gas.getOxygenAmount() * ambientPressure;
        exposure.cns += getCNSRatePerMinute(ppo2) * timeMinutes;
        exposure.otu += getOTURatePerMinute(ppo2) * timeMinutes;
    }

    private double getCNSRatePerMinute(double ppo2) {
        if (ppo2 <= 0.50) return 0.0;
        if (ppo2 <= 0.60) return 0.139;
        if (ppo2 <= 0.70) return 0.175;
        if (ppo2 <= 0.80) return 0.222;
        if (ppo2 <= 0.90) return 0.278;
        if (ppo2 <= 1.00) return 0.333;
        if (ppo2 <= 1.10) return 0.417;
        if (ppo2 <= 1.20) return 0.476;
        if (ppo2 <= 1.25) return 0.513;
        if (ppo2 <= 1.30) return 0.556;
        if (ppo2 <= 1.35) return 0.606;
        if (ppo2 <= 1.40) return 0.667;
        if (ppo2 <= 1.45) return 0.741;
        if (ppo2 <= 1.50) return 0.833;
        if (ppo2 <= 1.55) return 1.220;
        if (ppo2 <= 1.60) return 2.222;
        return 5.0;
    }

    private double getOTURatePerMinute(double ppo2) {
        if (ppo2 <= 0.50) return 0.0;
        return Math.pow((ppo2 - 0.50) / 0.50, 0.83);
    }

    private static class OxygenExposure {
        double cns = 0;
        double otu = 0;
    }

    private static class DecoResult {
        double totalAscentTimeSec = 0;
        double totalDecoTimeSec = 0;
        Map<Integer, Double> stopTimes = new LinkedHashMap<>();
        void addStopTime(int depthM, double timeSec) { stopTimes.merge(depthM, timeSec, Double::sum); }
    }

    private void printDecoPlan() {
        System.out.println("\n=== OC Dive Plan ===");
        System.out.printf("Depth: %dm, Time: %dmin, RMV: %d L/min%n", depth, time, rmv);
        System.out.printf("GF: %d/%d%n",
                (int)(settings.getGf().getLow() * 100), (int)(settings.getGf().getHigh() * 100));
        System.out.println();
        System.out.println("Stp  Tme  Run  Gas    Qty");
        System.out.println("─────────────────────────────");

        for (DecoPlanEntry entry : decoEntries) {
            System.out.printf("%3d  %3s  %3d  %s  %4d%n",
                    (int) entry.getDepthMeter(), entry.getTimeString(), (int) entry.getRuntimeMinutes(),
                    entry.getGasString(), (int) entry.getGasQty());
        }

        System.out.println("\nGas Usage:");
        for (Map.Entry<String, Integer> entry : gasUsage.entrySet()) {
            System.out.printf("  %s: %d L%n", entry.getKey(), entry.getValue());
        }
        System.out.printf("%nRun: %d min, Deco: %d min, CNS: %d%%, OTU: %d%n",
                totalRuntime, totalDecoTime, cnsPercent, otuValue);
    }

    private JPanel createInputPanel() {
        JPanel panel = new JPanel();
        panel.setBackground(BG_COLOR);
        panel.setLayout(new BoxLayout(panel, BoxLayout.Y_AXIS));
        panel.setBorder(BorderFactory.createEmptyBorder(10, 40, 10, 40));
        return panel;
    }

    private JLabel createLabel(String text, Color color, int fontSize) {
        JLabel label = new JLabel(text);
        label.setForeground(color);
        label.setFont(new java.awt.Font("Monospaced", java.awt.Font.PLAIN, fontSize));
        label.setAlignmentX(Component.CENTER_ALIGNMENT);
        return label;
    }

    private JLabel createTableHeader(String text) {
        JLabel label = new JLabel(text, SwingConstants.CENTER);
        label.setForeground(CYAN_COLOR);
        label.setFont(new java.awt.Font("Monospaced", java.awt.Font.BOLD, 12));
        label.setBorder(BorderFactory.createMatteBorder(0, 0, 1, 0, CYAN_COLOR));
        return label;
    }

    private JLabel createTableCell(String text) {
        JLabel label = new JLabel(text, SwingConstants.CENTER);
        label.setForeground(WHITE_COLOR);
        label.setFont(new java.awt.Font("Monospaced", java.awt.Font.PLAIN, 12));
        return label;
    }
}


class DiveDisplay {
    private OledHw display;
    private final int DISPLAYWIDTH = display.pixelsizex;
    private final int DISPLAYHEIGT = display.pixelsizey;
    public Font fontTahoma22pB = new Font( new ArialNarrow12pB().fontInfo);
    public Font fontArialNarrow12pB = new Font( new ArialNarrow12pB().fontInfo);
    private PixelBuffer pixelBuffer = new PixelBuffer(DISPLAYWIDTH, DISPLAYHEIGT);
    private PixelBuffer screenBuffer = new PixelBuffer(DISPLAYWIDTH, DISPLAYHEIGT);

    private int [] color = new int[256];

    public ColorPalette colors = new ColorPalette();

    public DiveDisplay(JPanel panel){
        this.display = new OledHw(panel);

        int i=0;
        for (int f=0; f<100;f+=7){
            color[i] = hsvToRgbInt(0, 0, f);
            i++;
        }
        color[i]= hsvToRgbInt(0,0,100);
        i++;
        for (int h=0; h<360; h+=24){
            for (int s=100; s>=25; s-=25) {
                for (int v=100; v >=25; v -= 25) {
                    color[i]=hsvToRgbInt(h,s,v);
                    i++;
                }
            }
        }
    }

    public int rgbToInt(int r, int g, int b){
        return (r << 16) | (g << 8) | b;
    }

    public int hsvToRgbInt(float h, float s, float v){
        float rHsv;
        float gHsv;
        float bHsv;
        int color;

        s=s/100;
        v=v/100;

        float c = s*v;
        float x = c * (1-Math.abs((h/60)%2-1));
        float m = v-c;

        if(h>=0 && h<=60){
            rHsv=c;
            gHsv=x;
            bHsv=0;
        }else if (h>60 && h<=120){
            rHsv=x;
            gHsv=c;
            bHsv=0;
        }else if (h>120 && h<=180){
            rHsv=0;
            gHsv=c;
            bHsv=x;
        }else if (h>180 && h<=240){
            rHsv=0;
            gHsv=x;
            bHsv=c;
        }else if (h>240 && h<=300){
            rHsv=x;
            gHsv=0;
            bHsv=c;
        }else if (h>300 && h<=360){
            rHsv=c;
            gHsv=0;
            bHsv=x;
        }else{
            System.out.println("H value out of bounds" + h);
            rHsv=0;
            gHsv=0;
            bHsv=0;
        }

        int r = (int) ((rHsv+m)*255);
        int g = (int) ((gHsv+m)*255);
        int b = (int) ((bHsv+m)*255);
        color = rgbToInt(r,g,b);
        return color;
    }

    public void setPixel(int x, int y, int color){
        pixelBuffer.setPixel(x,y,color);
    }

    public int drawChar(int posX, int posY, char c, int color, Font font){
        int courser = 0;
        if (c !=' ') {
            for (int y = 0; y < font.getCharHeight(c); y++) {
                for (int x = 0; x < font.getCharWidth(c); x++) {
                    if (font.getCharPixel(c, x, y)) {
                        setPixel(posX + x, posY + y-font.getCharHeight(c), color);
                    }
                }
            }
            courser= font.getCharWidth(c);
        }
        else{
            courser = font.fontInfo.getSpaceWidth();
        }
        return courser;
    }

    private int getStringLength(String s, Font font){
        int length=0;
        for (int i=0; i<s.length();i++){
            length = length + font.getCharWidth(s.charAt(i)) + 2;
        }
        return length;
    }

    public int drawString(int posX, int posY, String s, int color, Font font, boolean leftAligned){
        int courser = 0;
        int alignment = 0;
        if (!leftAligned) {
            alignment = -1 * getStringLength(s,font);
        }
        for (int i=0; i<s.length();i++){
            courser = courser + drawChar(posX+courser+alignment, posY,s.charAt(i),color, font) + 2;
        }
        return courser;
    }

    public void clearBuffer(){
        for (int y=0;y<DISPLAYHEIGT;y++) {
            for (int x = 0; x < DISPLAYWIDTH; x++) {
                pixelBuffer.setPixel(x,y,0);
            }
        }
    }

    public void antiAliasing(){
        for (int y=1;y<DISPLAYHEIGT-1;y++) {
            for (int x = 1; x < DISPLAYWIDTH-1; x++) {
                int pixel = pixelBuffer.getPixel(x,y);

                if (pixel != 0) {
                    int topLeft = pixelBuffer.getPixel(x-1,y-1);
                    int top = pixelBuffer.getPixel(x,y-1);
                    int topRight = pixelBuffer.getPixel(x+1,y-1);
                    int right = pixelBuffer.getPixel(x+1,y);
                    int botRight = pixelBuffer.getPixel(x+1,y+1);
                    int bottom = pixelBuffer.getPixel(x,y+1);
                    int botLeft = pixelBuffer.getPixel(x-1,y+1);
                    int left = pixelBuffer.getPixel(x-1,y);

                    if (pixel == left && pixel == top && pixel != topLeft) {
                        pixelBuffer.setPixel(x - 1, y - 1, pixel+2);
                    }

                    if (pixel == right && pixel == top && pixel != topRight) {
                        pixelBuffer.setPixel(x + 1, y - 1, pixel+2);
                    }

                    if (pixel == left && pixel == bottom && pixel != botLeft) {
                        pixelBuffer.setPixel(x - 1, y + 1, pixel+2);
                    }

                    if (pixel == right && pixel == bottom && pixel != botRight) {
                        pixelBuffer.setPixel(x + 1, y + 1, pixel+2);
                    }
                }
            }
        }
    }

    public void updateDisplay(){
        for (int y=0;y<DISPLAYHEIGT;y++){
            for (int x=0;x<DISPLAYWIDTH;x++){
                if(screenBuffer.getPixel(x,y) != pixelBuffer.getPixel(x,y)) {
                    screenBuffer.setPixel(x, y, pixelBuffer.getPixel(x, y));
                    display.drawPixel(x, y, color[pixelBuffer.getPixel(x,y)]);
                }
            }
        }
    }

    public void drawHorizontalLine(int line, int thickness, int color){
        for (int y=0;y<thickness;y++) {
            for (int x = 0; x < DISPLAYWIDTH; x++) {
                pixelBuffer.setPixel(x,line+y,color);
            }
        }
    }
}

class ColorPalette {
    public final int BLACK = 0;
    public final int RED = 16;
    public final int ORANGERED = 32;
    public final int ORANGEYELLOW = 48;
    public final int YELLOWGREEN = 64;
    public final int GREEN = 80;
    public final int DARKGREEN = 96;
    public final int GREENBLUE = 112;
    public final int BLUEGREEN = 128;
    public final int LIGHTBLUE = 144;
    public final int BLUE = 160;
    public final int DARKBLUE = 176;
    public final int BLUEPURPLE = 192;
    public final int PURPLE = 208;
    public final int PINK = 224;
    public final int REDPINK = 240;
}

class OledHw {
    public static int pixelsizex = 160;
    public static int pixelsizey = 128;
    private JPanel panel = new JPanel();
    FrameBufferPanel screen = new FrameBufferPanel(pixelsizex, pixelsizey, pixelsizex*0.21*4, pixelsizey*0.21*4);

    public OledHw(JPanel panel){
        this.panel = panel;
        initialize();
    }

    private void initialize(){
        panel.add(screen);
    }

    public void drawPixel(int x, int y, int color){
        int[] pixels = screen.lock();
        pixels[y * pixelsizex + x] = color;
        screen.update(pixels);
    }
}

class FrameBufferPanel extends JPanel {
    private final int width;
    private final int height;
    private int realWidth;
    private int realHeight;
    private final BufferedImage image;
    private final int[] pixels;

    public FrameBufferPanel(int width, int height, double widthmm, double heightmm) {
        this.width = width;
        this.height = height;
        this.realWidth = (int) (widthmm/0.18);
        this.realHeight = (int) (heightmm/0.18);
        setPreferredSize(new Dimension(realWidth, realHeight));
        image = new BufferedImage(this.width, this.height, BufferedImage.TYPE_INT_RGB);
        pixels = new int[this.width * this.height];
    }

    @Override
    public void paint(Graphics graphics) {
        final Image scaledInstance = image.getScaledInstance(realWidth, realHeight, Image.SCALE_FAST);
        graphics.drawImage(scaledInstance, 0, 0, realWidth, realHeight, null);
    }

    public int rgbToInt(int r, int g, int b){
        return (r << 16) | (g << 8) | b;
    }

    public int[] lock() {
        return pixels;
    }

    public void updatePixel(int x, int y, int color){ }

    public void update(int[] pixels) {
        if (pixels != this.pixels) {
            throw new IllegalArgumentException("Should update with locked pixel array");
        }
        image.setRGB(0, 0, width, height, pixels, 0, width);
        this.repaint();
    }
}

class PixelBuffer {
    private int width;
    private int height;
    private int[] pixelBuffer;

    public PixelBuffer(int width, int height) {
        this.width = width;
        this.height = height;
        pixelBuffer = new int[this.width* this.height];
    }

    private int getArrayPosition(int x, int y){
        return y*width+x;
    }

    private boolean isOutOfBounds(int x, int y){
        boolean value = false;
        if (x>= width)  value = true;
        if (x<0)        value = true;
        if (y>=height)  value = true;
        if (y<0)        value = true;
        return value;
    }

    public void setPixel(int x, int y, int color){
        if (!isOutOfBounds(x,y)){
            pixelBuffer[getArrayPosition(x,y)] = color;
        }
    }

    public int getPixel(int x, int y){
        return pixelBuffer[getArrayPosition(x,y)];
    }
}

class PressureSensor {
    private PressureSensorHw pressureSensorHw;
    private static final double range = 14.0;
    private static final int zeroOutput = 0x666;
    private static final int maxPressureOutput = 0x399A;

    public PressureSensor(JPanel panel, DiveSettings settings){
        this.pressureSensorHw = new PressureSensorHw(panel);
        updateEnvironment(settings);
    }

    public double getPressure(){
        double pressure = 0;
        pressure = (pressureSensorHw.getPressure()-zeroOutput)*range/(maxPressureOutput-zeroOutput);
        return pressure;
    }

    public void updateEnvironment(DiveSettings settings) {
        pressureSensorHw.updateEnvironment(settings);
    }
}

class PressureSensorHw {
    private JPanel panel;
    private double surfacePressure = 1.0;
    private double depthPerBar = DiveSettings.WaterDensity.EN13319.getMetersPerBar();

    int initial =(int) (1.0*13108/14+0x666);
    JSlider pressureSlider = new JSlider(JSlider.VERTICAL, 0x666, 0x399A, initial);

    private JTextField depthInput;
    private JLabel depthLabel;
    private boolean updatingFromSlider = false;
    private boolean updatingFromInput = false;

    private javax.swing.Timer simTimer;
    private double simStartDepth;
    private double simTargetDepth;
    private double simCurrentDepth;
    private int simElapsedSeconds;
    private int simTotalSeconds;
    private int simPhase;
    private int simBottomTime;
    private JButton simButton;
    private JLabel simStatusLabel;

    private static final double RANGE = 14.0;
    private static final int ZERO_OUTPUT = 0x666;
    private static final int MAX_PRESSURE_OUTPUT = 0x399A;

    public PressureSensorHw(JPanel panel){
        this.panel = panel;
        panel.setLayout(new BorderLayout());

        JPanel sliderPanel = new JPanel(new BorderLayout());
        pressureSlider.setInverted(true);
        sliderPanel.add(pressureSlider, BorderLayout.CENTER);

        JPanel inputPanel = new JPanel();
        inputPanel.setLayout(new BoxLayout(inputPanel, BoxLayout.Y_AXIS));

        depthLabel = new JLabel("深度(m):");
        depthLabel.setAlignmentX(Component.CENTER_ALIGNMENT);

        depthInput = new JTextField(5);
        depthInput.setMaximumSize(new Dimension(80, 25));
        depthInput.setAlignmentX(Component.CENTER_ALIGNMENT);
        depthInput.setText("0.0");

        JButton setButton = new JButton("设置");
        setButton.setAlignmentX(Component.CENTER_ALIGNMENT);

        simButton = new JButton("模拟潜水");
        simButton.setAlignmentX(Component.CENTER_ALIGNMENT);

        simStatusLabel = new JLabel("");
        simStatusLabel.setAlignmentX(Component.CENTER_ALIGNMENT);
        simStatusLabel.setForeground(Color.BLUE);

        inputPanel.add(Box.createVerticalStrut(10));
        inputPanel.add(depthLabel);
        inputPanel.add(Box.createVerticalStrut(5));
        inputPanel.add(depthInput);
        inputPanel.add(Box.createVerticalStrut(5));
        inputPanel.add(setButton);
        inputPanel.add(Box.createVerticalStrut(10));
        inputPanel.add(simButton);
        inputPanel.add(Box.createVerticalStrut(5));
        inputPanel.add(simStatusLabel);
        inputPanel.add(Box.createVerticalStrut(10));

        panel.add(sliderPanel, BorderLayout.CENTER);
        panel.add(inputPanel, BorderLayout.SOUTH);

        simButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                if (simTimer != null && simTimer.isRunning()) {
                    stopSimulation();
                } else {
                    showSimulationDialog();
                }
            }
        });

        pressureSlider.addChangeListener(new ChangeListener() {
            @Override
            public void stateChanged(ChangeEvent e) {
                if (!updatingFromInput) {
                    updatingFromSlider = true;
                    double depth = sliderValueToDepth(pressureSlider.getValue());
                    depthInput.setText(String.format("%.1f", depth));
                    updatingFromSlider = false;
                }
            }
        });

        setButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                setDepthFromInput();
            }
        });

        depthInput.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                setDepthFromInput();
            }
        });
    }

    private void setDepthFromInput() {
        try {
            double depth = Double.parseDouble(depthInput.getText().trim());
            if (depth < 0) depth = 0;
            if (depth > 130) depth = 130;

            updatingFromInput = true;
            int sliderValue = depthToSliderValue(depth);
            pressureSlider.setValue(sliderValue);
            updatingFromInput = false;
        } catch (NumberFormatException ex) {
            JOptionPane.showMessageDialog(panel, "请输入有效的数字", "输入错误", JOptionPane.ERROR_MESSAGE);
        }
    }

    private int depthToSliderValue(double depthInMeters) {
        double pressureInBar = surfacePressure + depthInMeters / depthPerBar;
        int sliderValue = (int) (pressureInBar * (MAX_PRESSURE_OUTPUT - ZERO_OUTPUT) / RANGE + ZERO_OUTPUT);
        return Math.max(ZERO_OUTPUT, Math.min(MAX_PRESSURE_OUTPUT, sliderValue));
    }

    private double sliderValueToDepth(int sliderValue) {
        double pressureInBar = (sliderValue - ZERO_OUTPUT) * RANGE / (MAX_PRESSURE_OUTPUT - ZERO_OUTPUT);
        double depth = (pressureInBar - surfacePressure) * depthPerBar;
        return Math.max(0, depth);
    }

    public void updateEnvironment(DiveSettings settings) {
        double currentDepth = sliderValueToDepth(pressureSlider.getValue());
        this.surfacePressure = settings.getSurfacePressure();
        this.depthPerBar = settings.getDepthPerBar();
        updatingFromInput = true;
        pressureSlider.setValue(depthToSliderValue(currentDepth));
        depthInput.setText(String.format("%.1f", currentDepth));
        updatingFromInput = false;
    }

    public int getPressure(){
        return pressureSlider.getValue();
    }

    private void showSimulationDialog() {
        JDialog dialog = new JDialog((Frame) SwingUtilities.getWindowAncestor(panel), "模拟潜水", true);
        dialog.setLayout(new BorderLayout());
        dialog.setSize(320, 280);
        dialog.setLocationRelativeTo(panel);

        JPanel mainPanel = new JPanel();
        mainPanel.setLayout(new BoxLayout(mainPanel, BoxLayout.Y_AXIS));
        mainPanel.setBorder(BorderFactory.createEmptyBorder(15, 20, 15, 20));

        JPanel depthPanel = new JPanel(new FlowLayout(FlowLayout.LEFT));
        depthPanel.add(new JLabel("目标深度 (m):"));
        final JTextField targetDepthField = new JTextField("32.3", 6);
        depthPanel.add(targetDepthField);

        JPanel descentPanel = new JPanel(new FlowLayout(FlowLayout.LEFT));
        descentPanel.add(new JLabel("下潜时间 (s):"));
        final JTextField descentTimeField = new JTextField("20", 6);
        descentPanel.add(descentTimeField);

        JPanel bottomPanel = new JPanel(new FlowLayout(FlowLayout.LEFT));
        bottomPanel.add(new JLabel("底部停留 (s):"));
        final JTextField bottomTimeField = new JTextField("0", 6);
        bottomPanel.add(bottomTimeField);

        JPanel ascentPanel = new JPanel(new FlowLayout(FlowLayout.LEFT));
        ascentPanel.add(new JLabel("上升时间 (s):"));
        final JTextField ascentTimeField = new JTextField("0", 6);
        ascentPanel.add(ascentTimeField);
        ascentPanel.add(new JLabel("(0=不上升)"));

        JLabel infoLabel = new JLabel("<html><font color='gray'>设置参数后点击开始，<br>将自动模拟下潜过程</font></html>");
        infoLabel.setAlignmentX(Component.CENTER_ALIGNMENT);

        mainPanel.add(depthPanel);
        mainPanel.add(descentPanel);
        mainPanel.add(bottomPanel);
        mainPanel.add(ascentPanel);
        mainPanel.add(Box.createVerticalStrut(15));
        mainPanel.add(infoLabel);

        JPanel buttonPanel = new JPanel(new FlowLayout(FlowLayout.CENTER));
        JButton startButton = new JButton("开始模拟");
        JButton cancelButton = new JButton("取消");

        startButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                try {
                    double targetDepth = Double.parseDouble(targetDepthField.getText().trim());
                    int descentTime = Integer.parseInt(descentTimeField.getText().trim());
                    int bottomTime = Integer.parseInt(bottomTimeField.getText().trim());
                    int ascentTime = Integer.parseInt(ascentTimeField.getText().trim());

                    if (targetDepth <= 0 || targetDepth > 130) {
                        JOptionPane.showMessageDialog(dialog, "深度范围: 1-130m", "输入错误", JOptionPane.ERROR_MESSAGE);
                        return;
                    }
                    if (descentTime <= 0 || descentTime > 600) {
                        JOptionPane.showMessageDialog(dialog, "下潜时间范围: 1-600s", "输入错误", JOptionPane.ERROR_MESSAGE);
                        return;
                    }

                    dialog.dispose();
                    startSimulation(targetDepth, descentTime, bottomTime, ascentTime);

                } catch (NumberFormatException ex) {
                    JOptionPane.showMessageDialog(dialog, "请输入有效的数字", "输入错误", JOptionPane.ERROR_MESSAGE);
                }
            }
        });

        cancelButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                dialog.dispose();
            }
        });

        buttonPanel.add(startButton);
        buttonPanel.add(cancelButton);

        dialog.add(mainPanel, BorderLayout.CENTER);
        dialog.add(buttonPanel, BorderLayout.SOUTH);
        dialog.setVisible(true);
    }

    private void startSimulation(double targetDepth, int descentTime, int bottomTime, int ascentTime) {
        simStartDepth = sliderValueToDepth(pressureSlider.getValue());
        simTargetDepth = targetDepth;
        simCurrentDepth = simStartDepth;
        simTotalSeconds = descentTime;
        simBottomTime = bottomTime;
        simElapsedSeconds = 0;
        simPhase = 0;

        simButton.setText("停止模拟");

        final int finalAscentTime = ascentTime;

        simTimer = new javax.swing.Timer(1000, new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                simElapsedSeconds++;

                if (simPhase == 0) {
                    double progress = (double) simElapsedSeconds / simTotalSeconds;
                    simCurrentDepth = simStartDepth + (simTargetDepth - simStartDepth) * progress;
                    simStatusLabel.setText(String.format("下潜中 %d/%ds", simElapsedSeconds, simTotalSeconds));

                    if (simElapsedSeconds >= simTotalSeconds) {
                        simCurrentDepth = simTargetDepth;
                        if (simBottomTime > 0) {
                            simPhase = 1;
                            simElapsedSeconds = 0;
                            simTotalSeconds = simBottomTime;
                        } else if (finalAscentTime > 0) {
                            simPhase = 2;
                            simElapsedSeconds = 0;
                            simTotalSeconds = finalAscentTime;
                        } else {
                            stopSimulation();
                            simStatusLabel.setText("完成 - 停在底部");
                            return;
                        }
                    }
                } else if (simPhase == 1) {
                    simStatusLabel.setText(String.format("底部停留 %d/%ds", simElapsedSeconds, simTotalSeconds));

                    if (simElapsedSeconds >= simTotalSeconds) {
                        if (finalAscentTime > 0) {
                            simPhase = 2;
                            simElapsedSeconds = 0;
                            simTotalSeconds = finalAscentTime;
                        } else {
                            stopSimulation();
                            simStatusLabel.setText("完成 - 停在底部");
                            return;
                        }
                    }
                } else if (simPhase == 2) {
                    double progress = (double) simElapsedSeconds / simTotalSeconds;
                    simCurrentDepth = simTargetDepth * (1 - progress);
                    simStatusLabel.setText(String.format("上升中 %d/%ds", simElapsedSeconds, simTotalSeconds));

                    if (simElapsedSeconds >= simTotalSeconds) {
                        simCurrentDepth = 0;
                        stopSimulation();
                        simStatusLabel.setText("模拟完成");
                        return;
                    }
                }

                updatingFromInput = true;
                pressureSlider.setValue(depthToSliderValue(simCurrentDepth));
                depthInput.setText(String.format("%.1f", simCurrentDepth));
                updatingFromInput = false;
            }
        });

        simTimer.start();
        System.out.println(String.format("=== 开始模拟潜水 ===%n目标深度: %.1fm%n下潜时间: %ds%n底部停留: %ds",
                targetDepth, descentTime, bottomTime));
    }

    private void stopSimulation() {
        if (simTimer != null) {
            simTimer.stop();
            simTimer = null;
        }
        simButton.setText("模拟潜水");
    }
}

class Font {
    public final FontInfo fontInfo;

    public Font(FontInfo fontInfo){
        this.fontInfo = fontInfo;
    }

    public int getCharWidth(char c){
        if(c != ' ') {
            return fontInfo.descriptors[c - fontInfo.getStartChar()][0];
        }else
            return fontInfo.getSpaceWidth();
    }

    public int getCharHeight(char c){
        return fontInfo.descriptors[c-fontInfo.getStartChar()][1];
    }

    public int getCharWidthBytes(char c){
        return (fontInfo.descriptors[c - fontInfo.getStartChar()][0] + 7) / 8;
    }

    private byte getCharByte(char c, int byteNumber){
        return fontInfo.bitmaps[fontInfo.descriptors[c-fontInfo.getStartChar()][2]+byteNumber];
    }

    private boolean getBitInByte(byte b, int bitNumber){
        boolean value=false;
        if ((b & (1<<bitNumber)) != 0){
            value = true;
        }
        return value;
    }

    private int getByteNumber(char c, int x, int y){
        return y*getCharWidthBytes(c)+x/8;
    }

    public boolean getCharPixel(char c, int x, int y){
        boolean value=false;
        if (getBitInByte(getCharByte(c, getByteNumber(c,x,y)), x%8)){
            value=true;
        }
        return value;
    }
}

class FontInfo {
    private final char startChar;
    private final char endChar;
    private final int spaceWidth;
    public final int [][] descriptors;
    public final byte [] bitmaps;

    public FontInfo(char startChar, char endChar, int spaceWidth, int[][] descriptors, byte[] bitmaps) {
        this.startChar = startChar;
        this.endChar = endChar;
        this.spaceWidth = spaceWidth;
        this.descriptors = descriptors;
        this.bitmaps = bitmaps;
    }

    public char getStartChar() { return startChar; }
    public char getEndChar() { return endChar; }
    public int getSpaceWidth() { return spaceWidth; }
}

class ArialNarrow12pB {
    public static final int[][] descriptors =
            {
                    {2, 15, 0}, 		// !
                    {5, 15, 15}, 		// "
                    {7, 15, 30}, 		// #
                    {7, 15, 45}, 		// $
                    {10, 15, 60}, 		// %
                    {8, 15, 90}, 		// &
                    {2, 15, 105}, 		// '
                    {4, 15, 120}, 		// (
                    {4, 15, 135}, 		// )
                    {5, 15, 150}, 		// *
                    {6, 15, 165}, 		// +
                    {2, 15, 180}, 		// ,
                    {4, 15, 195}, 		// -
                    {2, 15, 210}, 		// .
                    {4, 15, 225}, 		// /
                    {6, 15, 240}, 		// 0
                    {4, 15, 255}, 		// 1
                    {6, 15, 270}, 		// 2
                    {6, 15, 285}, 		// 3
                    {7, 15, 300}, 		// 4
                    {6, 15, 315}, 		// 5
                    {5, 15, 330}, 		// 6
                    {5, 15, 345}, 		// 7
                    {6, 15, 360}, 		// 8
                    {6, 15, 375}, 		// 9
                    {2, 15, 390}, 		// :
                    {2, 15, 405}, 		// ;
                    {6, 15, 420}, 		// <
                    {6, 15, 435}, 		// =
                    {6, 15, 450}, 		// >
                    {6, 15, 465}, 		// ?
                    {13, 15, 480}, 		// @
                    {9, 15, 510}, 		// A
                    {7, 15, 540}, 		// B
                    {8, 15, 555}, 		// C
                    {7, 15, 570}, 		// D
                    {7, 15, 585}, 		// E
                    {6, 15, 600}, 		// F
                    {8, 15, 615}, 		// G
                    {7, 15, 630}, 		// H
                    {2, 15, 645}, 		// I
                    {6, 15, 660}, 		// J
                    {8, 15, 675}, 		// K
                    {7, 15, 690}, 		// L
                    {9, 15, 705}, 		// M
                    {7, 15, 735}, 		// N
                    {8, 15, 750}, 		// O
                    {7, 15, 765}, 		// P
                    {9, 15, 780}, 		// Q
                    {8, 15, 810}, 		// R
                    {7, 15, 825}, 		// S
                    {8, 15, 840}, 		// T
                    {7, 15, 855}, 		// U
                    {9, 15, 870}, 		// V
                    {11, 15, 900}, 		// W
                    {9, 15, 930}, 		// X
                    {8, 15, 960}, 		// Y
                    {7, 15, 975}, 		// Z
                    {3, 15, 990}, 		// [
                    {4, 15, 1005}, 		// \
                    {3, 15, 1020}, 		// ]
                    {7, 15, 1035}, 		// ^
                    {7, 15, 1050}, 		// _
                    {3, 15, 1065}, 		// `
                    {6, 15, 1080}, 		// a
                    {6, 15, 1095}, 		// b
                    {5, 15, 1110}, 		// c
                    {6, 15, 1125}, 		// d
                    {5, 15, 1140}, 		// e
                    {4, 15, 1155}, 		// f
                    {6, 15, 1170}, 		// g
                    {6, 15, 1185}, 		// h
                    {2, 15, 1200}, 		// i
                    {4, 15, 1215}, 		// j
                    {6, 15, 1230}, 		// k
                    {2, 15, 1245}, 		// l
                    {10, 15, 1260}, 		// m
                    {6, 15, 1290}, 		// n
                    {6, 15, 1305}, 		// o
                    {6, 15, 1320}, 		// p
                    {6, 15, 1335}, 		// q
                    {4, 15, 1350}, 		// r
                    {5, 15, 1365}, 		// s
                    {4, 15, 1380}, 		// t
                    {6, 15, 1395}, 		// u
                    {7, 15, 1410}, 		// v
                    {9, 15, 1425}, 		// w
                    {6, 15, 1455}, 		// x
                    {7, 15, 1470}, 		// y
                    {6, 15, 1485}, 		// z
                    {4, 15, 1500}, 		// {
                    {2, 15, 1515}, 		// |
                    {4, 15, 1530}, 		// }
                    {6, 15, 1545}, 		// ~
                    {0, 0, 0}, 		// 
                    {0, 0, 0}, 		// €
                    {0, 0, 0}, 		// 
                    {0, 0, 0}, 		// ‚
                    {0, 0, 0}, 		// ƒ
                    {0, 0, 0}, 		// „
                    {0, 0, 0}, 		// …
                    {0, 0, 0}, 		// †
                    {0, 0, 0}, 		// ‡
                    {0, 0, 0}, 		// ˆ
                    {0, 0, 0}, 		// ‰
                    {0, 0, 0}, 		// Š
                    {0, 0, 0}, 		// ‹
                    {0, 0, 0}, 		// Œ
                    {0, 0, 0}, 		// 
                    {0, 0, 0}, 		// Ž
                    {0, 0, 0}, 		// 
                    {0, 0, 0}, 		// 
                    {0, 0, 0}, 		// ‘
                    {0, 0, 0}, 		// ’
                    {0, 0, 0}, 		// “
                    {0, 0, 0}, 		// ”
                    {0, 0, 0}, 		// •
                    {0, 0, 0}, 		// –
                    {0, 0, 0}, 		// —
                    {0, 0, 0}, 		// ˜
                    {0, 0, 0}, 		// ™
                    {0, 0, 0}, 		// š
                    {0, 0, 0}, 		// ›
                    {0, 0, 0}, 		// œ
                    {0, 0, 0}, 		// 
                    {0, 0, 0}, 		// ž
                    {0, 0, 0}, 		// Ÿ
                    {0, 0, 0}, 		//  
                    {0, 0, 0}, 		// ¡
                    {0, 0, 0}, 		// ¢
                    {0, 0, 0}, 		// £
                    {0, 0, 0}, 		// ¤
                    {0, 0, 0}, 		// ¥
                    {0, 0, 0}, 		// ¦
                    {0, 0, 0}, 		// §
                    {0, 0, 0}, 		// ¨
                    {0, 0, 0}, 		// ©
                    {0, 0, 0}, 		// ª
                    {0, 0, 0}, 		// «
                    {0, 0, 0}, 		// ¬
                    {0, 0, 0}, 		// ­
                    {0, 0, 0}, 		// ®
                    {0, 0, 0}, 		// ¯
                    {5, 15, 1560}, 		// °
                    {0, 0, 0}, 		// ±
                    {4, 15, 1575}, 		// ²
                    {4, 15, 1590}, 		// ³
            };

    public static byte bitmaps[] =
            {
                    // @0 '!' (2 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000000, //
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @15 '"' (5 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00011011, // ## ##
                    (byte) 0b00011011, // ## ##
                    (byte) 0b00011011, // ## ##
                    (byte) 0b00011011, // ## ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @30 '#' (7 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b01001000, //    #  #
                    (byte) 0b01001000, //    #  #
                    (byte) 0b00100100, //   #  #
                    (byte) 0b01111111, // #######
                    (byte) 0b01111111, // #######
                    (byte) 0b00010010, //  #  #
                    (byte) 0b01111111, // #######
                    (byte) 0b01111111, // #######
                    (byte) 0b00010010, //  #  #
                    (byte) 0b00001001, // #  #
                    (byte) 0b00001001, // #  #
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @45 '$' (7 pixels wide)
                    (byte) 0b00001000, //    #
                    (byte) 0b00111110, //  #####
                    (byte) 0b00111111, // ######
                    (byte) 0b01101011, // ## # ##
                    (byte) 0b00001011, // ## #
                    (byte) 0b00001111, // ####
                    (byte) 0b00111110, //  #####
                    (byte) 0b01111000, //    ####
                    (byte) 0b01101000, //    # ##
                    (byte) 0b01101011, // ## # ##
                    (byte) 0b01111110, //  ######
                    (byte) 0b00111110, //  #####
                    (byte) 0b00001000, //    #
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @60 '%' (10 pixels wide)
                    (byte) 0b00000000, (byte) 0b00000000, //
                    (byte) 0b10000110, (byte) 0b00000000, //  ##    #
                    (byte) 0b10001001, (byte) 0b00000000, // #  #   #
                    (byte) 0b01001001, (byte) 0b00000000, // #  #  #
                    (byte) 0b01001001, (byte) 0b00000000, // #  #  #
                    (byte) 0b00101001, (byte) 0b00000000, // #  # #
                    (byte) 0b10100110, (byte) 0b00000001, //  ##  # ##
                    (byte) 0b01100000, (byte) 0b00000010, //      ##  #
                    (byte) 0b01010000, (byte) 0b00000010, //     # #  #
                    (byte) 0b01010000, (byte) 0b00000010, //     # #  #
                    (byte) 0b01001000, (byte) 0b00000010, //    #  #  #
                    (byte) 0b10001000, (byte) 0b00000001, //    #   ##
                    (byte) 0b00000000, (byte) 0b00000000, //
                    (byte) 0b00000000, (byte) 0b00000000, //
                    (byte) 0b00000000, (byte) 0b00000000, //

                    // @90 '&' (8 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00011100, //   ###
                    (byte) 0b00111110, //  #####
                    (byte) 0b00110110, //  ## ##
                    (byte) 0b00111110, //  #####
                    (byte) 0b00001100, //   ##
                    (byte) 0b00011110, //  ####
                    (byte) 0b01011011, // ## ## #
                    (byte) 0b01110011, // ##  ###
                    (byte) 0b01110011, // ##  ###
                    (byte) 0b11111111, // ########
                    (byte) 0b01011110, //  #### #
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @105 ''' (2 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @120 '(' (4 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00001000, //    #
                    (byte) 0b00000100, //   #
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000100, //   #
                    (byte) 0b00001000, //    #

                    // @135 ')' (4 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000001, // #
                    (byte) 0b00000010, //  #
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000100, //   #
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000010, //  #
                    (byte) 0b00000001, // #

                    // @150 '*' (5 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000100, //   #
                    (byte) 0b00010101, // # # #
                    (byte) 0b00011111, // #####
                    (byte) 0b00000100, //   #
                    (byte) 0b00001010, //  # #
                    (byte) 0b00001010, //  # #
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @165 '+' (6 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00111111, // ######
                    (byte) 0b00111111, // ######
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @180 ',' (2 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000010, //  #
                    (byte) 0b00000010, //  #
                    (byte) 0b00000001, // #

                    // @195 '-' (4 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00001111, // ####
                    (byte) 0b00001111, // ####
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @210 '.' (2 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @225 '/' (4 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @240 '0' (6 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00011110, //  ####
                    (byte) 0b00011110, //  ####
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00011110, //  ####
                    (byte) 0b00011110, //  ####
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @255 '1' (4 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001110, //  ###
                    (byte) 0b00001111, // ####
                    (byte) 0b00001101, // # ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @270 '2' (6 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00011100, //   ###
                    (byte) 0b00111110, //  #####
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110000, //      ##
                    (byte) 0b00011000, //    ##
                    (byte) 0b00011100, //   ###
                    (byte) 0b00001110, //  ###
                    (byte) 0b00000110, //  ##
                    (byte) 0b00111111, // ######
                    (byte) 0b00111111, // ######
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @285 '3' (6 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00011110, //  ####
                    (byte) 0b00111111, // ######
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110000, //      ##
                    (byte) 0b00011100, //   ###
                    (byte) 0b00011100, //   ###
                    (byte) 0b00110000, //      ##
                    (byte) 0b00110000, //      ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00011111, // #####
                    (byte) 0b00011110, //  ####
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @300 '4' (7 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00110000, //      ##
                    (byte) 0b00111000, //    ###
                    (byte) 0b00111000, //    ###
                    (byte) 0b00111100, //   ####
                    (byte) 0b00110100, //   # ##
                    (byte) 0b00110110, //  ## ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b01111111, // #######
                    (byte) 0b01111111, // #######
                    (byte) 0b00110000, //      ##
                    (byte) 0b00110000, //      ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @315 '5' (6 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00111110, //  #####
                    (byte) 0b00111110, //  #####
                    (byte) 0b00000110, //  ##
                    (byte) 0b00011011, // ## ##
                    (byte) 0b00111111, // ######
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110000, //      ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00011110, //  ####
                    (byte) 0b00011110, //  ####
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @330 '6' (5 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00001110, //  ###
                    (byte) 0b00011110, //  ####
                    (byte) 0b00011011, // ## ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00001111, // ####
                    (byte) 0b00011111, // #####
                    (byte) 0b00011011, // ## ##
                    (byte) 0b00011011, // ## ##
                    (byte) 0b00011011, // ## ##
                    (byte) 0b00011110, //  ####
                    (byte) 0b00001110, //  ###
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @345 '7' (5 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00011111, // #####
                    (byte) 0b00011111, // #####
                    (byte) 0b00011000, //    ##
                    (byte) 0b00001000, //    #
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00000100, //   #
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @360 '8' (6 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00011110, //  ####
                    (byte) 0b00111111, // ######
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00011110, //  ####
                    (byte) 0b00011110, //  ####
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00111111, // ######
                    (byte) 0b00011110, //  ####
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @375 '9' (6 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00001110, //  ###
                    (byte) 0b00011111, // #####
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00111111, // ######
                    (byte) 0b00111110, //  #####
                    (byte) 0b00110000, //      ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00011111, // #####
                    (byte) 0b00001110, //  ###
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @390 ':' (2 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @405 ';' (2 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000010, //  #
                    (byte) 0b00000010, //  #
                    (byte) 0b00000001, // #

                    // @420 '<' (6 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00100000, //      #
                    (byte) 0b00111000, //    ###
                    (byte) 0b00011100, //   ###
                    (byte) 0b00001111, // ####
                    (byte) 0b00000011, // ##
                    (byte) 0b00001111, // ####
                    (byte) 0b00011100, //   ###
                    (byte) 0b00111000, //    ###
                    (byte) 0b00100000, //      #
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @435 '=' (6 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00111111, // ######
                    (byte) 0b00111111, // ######
                    (byte) 0b00000000, //
                    (byte) 0b00111111, // ######
                    (byte) 0b00111111, // ######
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @450 '>' (6 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000001, // #
                    (byte) 0b00000111, // ###
                    (byte) 0b00001110, //  ###
                    (byte) 0b00111100, //   ####
                    (byte) 0b00110000, //      ##
                    (byte) 0b00111000, //    ###
                    (byte) 0b00001110, //  ###
                    (byte) 0b00000111, // ###
                    (byte) 0b00000001, // #
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @465 '?' (6 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00011110, //  ####
                    (byte) 0b00111111, // ######
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00111000, //    ###
                    (byte) 0b00011000, //    ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00000000, //
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @480 '@' (13 pixels wide)
                    (byte) 0b00000000, (byte) 0b00000000, //
                    (byte) 0b11110000, (byte) 0b00000011, //      ######
                    (byte) 0b11111000, (byte) 0b00000111, //    ########
                    (byte) 0b00001100, (byte) 0b00001100, //   ##      ##
                    (byte) 0b01100110, (byte) 0b00011011, //  ##  ## ## ##
                    (byte) 0b11110111, (byte) 0b00011011, // ### ###### ##
                    (byte) 0b10111011, (byte) 0b00011011, // ## ### ### ##
                    (byte) 0b00011011, (byte) 0b00011001, // ## ##   #  ##
                    (byte) 0b10011011, (byte) 0b00011001, // ## ##  ##  ##
                    (byte) 0b10011011, (byte) 0b00001101, // ## ##  ## ##
                    (byte) 0b11111011, (byte) 0b00001111, // ## #########
                    (byte) 0b01110110, (byte) 0b00010011, //  ## ### ##  #
                    (byte) 0b00001110, (byte) 0b00001000, //  ###        #
                    (byte) 0b11111100, (byte) 0b00000111, //   #########
                    (byte) 0b11110000, (byte) 0b00000011, //      ######

                    // @510 'A' (9 pixels wide)
                    (byte) 0b00000000, (byte) 0b00000000, //
                    (byte) 0b00111000, (byte) 0b00000000, //    ###
                    (byte) 0b00111000, (byte) 0b00000000, //    ###
                    (byte) 0b00111000, (byte) 0b00000000, //    ###
                    (byte) 0b01101100, (byte) 0b00000000, //   ## ##
                    (byte) 0b01101100, (byte) 0b00000000, //   ## ##
                    (byte) 0b01101100, (byte) 0b00000000, //   ## ##
                    (byte) 0b11000110, (byte) 0b00000000, //  ##   ##
                    (byte) 0b11111110, (byte) 0b00000000, //  #######
                    (byte) 0b11111110, (byte) 0b00000000, //  #######
                    (byte) 0b10000011, (byte) 0b00000001, // ##     ##
                    (byte) 0b10000011, (byte) 0b00000001, // ##     ##
                    (byte) 0b00000000, (byte) 0b00000000, //
                    (byte) 0b00000000, (byte) 0b00000000, //
                    (byte) 0b00000000, (byte) 0b00000000, //

                    // @540 'B' (7 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00111111, // ######
                    (byte) 0b01111111, // #######
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b00111111, // ######
                    (byte) 0b00111111, // ######
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01111111, // #######
                    (byte) 0b00111111, // ######
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @555 'C' (8 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00111100, //  ####
                    (byte) 0b01111110, //  ######
                    (byte) 0b11100110, //  ## ###
                    (byte) 0b01000011, // ##    #
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b01000011, // ##    #
                    (byte) 0b11100110, //  ## ###
                    (byte) 0b01111110, //  ######
                    (byte) 0b00111100, //  ####
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @570 'D' (7 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00011111, // #####
                    (byte) 0b00111111, // ######
                    (byte) 0b01110011, // ## ###
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01110011, // ## ###
                    (byte) 0b00111111, // ######
                    (byte) 0b00011111, // #####
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @585 'E' (7 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b01111111, // #######
                    (byte) 0b01111111, // #######
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b01111111, // #######
                    (byte) 0b01111111, // #######
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b01111111, // #######
                    (byte) 0b01111111, // #######
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @600 'F' (6 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00111111, // ######
                    (byte) 0b00111111, // ######
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00011111, // #####
                    (byte) 0b00011111, // #####
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @615 'G' (8 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00111100, //  ####
                    (byte) 0b01111110, //  ######
                    (byte) 0b11100110, //  ## ###
                    (byte) 0b11000011, // ##   ##
                    (byte) 0b00000011, // ##
                    (byte) 0b11110011, // ## ####
                    (byte) 0b11110011, // ## ####
                    (byte) 0b11000011, // ##   ##
                    (byte) 0b11000110, //  ##  ##
                    (byte) 0b11111110, //  #######
                    (byte) 0b00111100, //  ####
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @630 'H' (7 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01111111, // #######
                    (byte) 0b01111111, // #######
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @645 'I' (2 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @660 'J' (6 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00110000, //      ##
                    (byte) 0b00110000, //      ##
                    (byte) 0b00110000, //      ##
                    (byte) 0b00110000, //      ##
                    (byte) 0b00110000, //      ##
                    (byte) 0b00110000, //      ##
                    (byte) 0b00110000, //      ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00111111, // ######
                    (byte) 0b00011110, //  ####
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @675 'K' (8 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b11000011, // ##    ##
                    (byte) 0b01100011, // ##   ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00011011, // ## ##
                    (byte) 0b00111111, // ######
                    (byte) 0b00110111, // ### ##
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b11000011, // ##    ##
                    (byte) 0b11000011, // ##    ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @690 'L' (7 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b01111111, // #######
                    (byte) 0b01111111, // #######
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @705 'M' (9 pixels wide)
                    (byte) 0b00000000, (byte) 0b00000000, //
                    (byte) 0b11000111, (byte) 0b00000001, // ###   ###
                    (byte) 0b11000111, (byte) 0b00000001, // ###   ###
                    (byte) 0b11000111, (byte) 0b00000001, // ###   ###
                    (byte) 0b11101111, (byte) 0b00000001, // #### ####
                    (byte) 0b10101011, (byte) 0b00000001, // ## # # ##
                    (byte) 0b10101011, (byte) 0b00000001, // ## # # ##
                    (byte) 0b10101011, (byte) 0b00000001, // ## # # ##
                    (byte) 0b10101011, (byte) 0b00000001, // ## # # ##
                    (byte) 0b10111011, (byte) 0b00000001, // ## ### ##
                    (byte) 0b10011011, (byte) 0b00000001, // ## ##  ##
                    (byte) 0b10010011, (byte) 0b00000001, // ##  #  ##
                    (byte) 0b00000000, (byte) 0b00000000, //
                    (byte) 0b00000000, (byte) 0b00000000, //
                    (byte) 0b00000000, (byte) 0b00000000, //

                    // @735 'N' (7 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01100111, // ###  ##
                    (byte) 0b01100111, // ###  ##
                    (byte) 0b01100111, // ###  ##
                    (byte) 0b01101111, // #### ##
                    (byte) 0b01101011, // ## # ##
                    (byte) 0b01111011, // ## ####
                    (byte) 0b01111011, // ## ####
                    (byte) 0b01110011, // ##  ###
                    (byte) 0b01110011, // ##  ###
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @750 'O' (8 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00111100, //  ####
                    (byte) 0b01111110, //  ######
                    (byte) 0b11100110, //  ## ###
                    (byte) 0b11000011, // ##    ##
                    (byte) 0b11000011, // ##    ##
                    (byte) 0b11000011, // ##    ##
                    (byte) 0b11000011, // ##    ##
                    (byte) 0b11000011, // ##    ##
                    (byte) 0b01100110, //  ##  ##
                    (byte) 0b01111110, //  ######
                    (byte) 0b00111100, //  ####
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @765 'P' (7 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00111111, // ######
                    (byte) 0b01111111, // #######
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01111111, // #######
                    (byte) 0b00111111, // ######
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @780 'Q' (9 pixels wide)
                    (byte) 0b00000000, (byte) 0b00000000, //
                    (byte) 0b00111100, (byte) 0b00000000, //  ####
                    (byte) 0b01111110, (byte) 0b00000000, //  ######
                    (byte) 0b11100111, (byte) 0b00000000, // ###  ###
                    (byte) 0b11000011, (byte) 0b00000000, // ##    ##
                    (byte) 0b11000011, (byte) 0b00000000, // ##    ##
                    (byte) 0b11000011, (byte) 0b00000000, // ##    ##
                    (byte) 0b11000011, (byte) 0b00000000, // ##    ##
                    (byte) 0b11010011, (byte) 0b00000000, // ##  # ##
                    (byte) 0b01100110, (byte) 0b00000000, //  ##  ##
                    (byte) 0b11111110, (byte) 0b00000000, //  #######
                    (byte) 0b10111100, (byte) 0b00000001, //   #### ##
                    (byte) 0b00000000, (byte) 0b00000001, //         #
                    (byte) 0b00000000, (byte) 0b00000000, //
                    (byte) 0b00000000, (byte) 0b00000000, //

                    // @810 'R' (8 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00111111, // ######
                    (byte) 0b01111111, // #######
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01111111, // #######
                    (byte) 0b00111111, // ######
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b11000011, // ##    ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @825 'S' (7 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00111110, //  #####
                    (byte) 0b01111111, // #######
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b00001111, // ####
                    (byte) 0b00111110, //  #####
                    (byte) 0b01110000, //      ###
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01111111, // #######
                    (byte) 0b00111110, //  #####
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @840 'T' (8 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b11111111, // ########
                    (byte) 0b11111111, // ########
                    (byte) 0b00011000, //    ##
                    (byte) 0b00011000, //    ##
                    (byte) 0b00011000, //    ##
                    (byte) 0b00011000, //    ##
                    (byte) 0b00011000, //    ##
                    (byte) 0b00011000, //    ##
                    (byte) 0b00011000, //    ##
                    (byte) 0b00011000, //    ##
                    (byte) 0b00011000, //    ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @855 'U' (7 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01100011, // ##  ##
                    (byte) 0b01111110, //  ######
                    (byte) 0b00011100, //   ###
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @870 'V' (9 pixels wide)
                    (byte) 0b00000000, (byte) 0b00000000, //
                    (byte) 0b10000011, (byte) 0b00000001, // ##     ##
                    (byte) 0b10000011, (byte) 0b00000001, // ##     ##
                    (byte) 0b11000110, (byte) 0b00000000, //  ##   ##
                    (byte) 0b11000110, (byte) 0b00000000, //  ##   ##
                    (byte) 0b11000110, (byte) 0b00000000, //  ##   ##
                    (byte) 0b01101100, (byte) 0b00000000, //   ## ##
                    (byte) 0b01101100, (byte) 0b00000000, //   ## ##
                    (byte) 0b01101100, (byte) 0b00000000, //   ## ##
                    (byte) 0b00111000, (byte) 0b00000000, //    ###
                    (byte) 0b00111000, (byte) 0b00000000, //    ###
                    (byte) 0b00111000, (byte) 0b00000000, //    ###
                    (byte) 0b00000000, (byte) 0b00000000, //
                    (byte) 0b00000000, (byte) 0b00000000, //
                    (byte) 0b00000000, (byte) 0b00000000, //

                    // @900 'W' (11 pixels wide)
                    (byte) 0b00000000, (byte) 0b00000000, //
                    (byte) 0b00100011, (byte) 0b00000110, // ##   #   ##
                    (byte) 0b00100011, (byte) 0b00000110, // ##   #   ##
                    (byte) 0b01010011, (byte) 0b00000110, // ##  # #  ##
                    (byte) 0b01010110, (byte) 0b00000011, //  ## # # ##
                    (byte) 0b01010110, (byte) 0b00000011, //  ## # # ##
                    (byte) 0b01010110, (byte) 0b00000011, //  ## # # ##
                    (byte) 0b01010110, (byte) 0b00000011, //  ## # # ##
                    (byte) 0b01010110, (byte) 0b00000011, //  ## # # ##
                    (byte) 0b10001100, (byte) 0b00000001, //   ##   ##
                    (byte) 0b10001100, (byte) 0b00000001, //   ##   ##
                    (byte) 0b10001100, (byte) 0b00000001, //   ##   ##
                    (byte) 0b00000000, (byte) 0b00000000, //
                    (byte) 0b00000000, (byte) 0b00000000, //
                    (byte) 0b00000000, (byte) 0b00000000, //

                    // @930 'X' (9 pixels wide)
                    (byte) 0b00000000, (byte) 0b00000000, //
                    (byte) 0b10000011, (byte) 0b00000001, // ##     ##
                    (byte) 0b11000110, (byte) 0b00000000, //  ##   ##
                    (byte) 0b01101100, (byte) 0b00000000, //   ## ##
                    (byte) 0b01101100, (byte) 0b00000000, //   ## ##
                    (byte) 0b00111000, (byte) 0b00000000, //    ###
                    (byte) 0b00111000, (byte) 0b00000000, //    ###
                    (byte) 0b00111000, (byte) 0b00000000, //    ###
                    (byte) 0b01101100, (byte) 0b00000000, //   ## ##
                    (byte) 0b01101100, (byte) 0b00000000, //   ## ##
                    (byte) 0b11000110, (byte) 0b00000000, //  ##   ##
                    (byte) 0b10000011, (byte) 0b00000001, // ##     ##
                    (byte) 0b00000000, (byte) 0b00000000, //
                    (byte) 0b00000000, (byte) 0b00000000, //
                    (byte) 0b00000000, (byte) 0b00000000, //

                    // @960 'Y' (8 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b11000011, // ##   ##
                    (byte) 0b01100110, //  ##  ##
                    (byte) 0b01100110, //  ##  ##
                    (byte) 0b00111100, //  ####
                    (byte) 0b00111100, //  ####
                    (byte) 0b00011000, //    ##
                    (byte) 0b00011000, //    ##
                    (byte) 0b00011000, //    ##
                    (byte) 0b00011000, //    ##
                    (byte) 0b00011000, //    ##
                    (byte) 0b00011000, //    ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @975 'Z' (7 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b01111111, // #######
                    (byte) 0b01111111, // #######
                    (byte) 0b01100000, //      ##
                    (byte) 0b00110000, //     ##
                    (byte) 0b00011000, //    ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000011, // ##
                    (byte) 0b01111111, // #######
                    (byte) 0b01111111, // #######
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @990 '[' (3 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000111, // ###
                    (byte) 0b00000111, // ###
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000111, // ###
                    (byte) 0b00000111, // ###

                    // @1005 '\' (4 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @1020 ']' (3 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000111, // ###
                    (byte) 0b00000111, // ###
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000111, // ###
                    (byte) 0b00000111, // ###

                    // @1035 '^' (7 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00011100, //   ###
                    (byte) 0b00011100, //   ###
                    (byte) 0b00110110, //  ## ##
                    (byte) 0b00110110, //  ## ##
                    (byte) 0b00110110, //  ## ##
                    (byte) 0b01100011, // ##   ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @1050 '_' (7 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b01111111, // #######
                    (byte) 0b00000000, //

                    // @1065 '`' (3 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000011, // ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @1080 'a' (6 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00011110, //  ####
                    (byte) 0b00111111, // ######
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00111100, //  ####
                    (byte) 0b00111111, // ######
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00111111, // ######
                    (byte) 0b00110110, //  ## ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @1095 'b' (6 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00011111, // #####
                    (byte) 0b00111111, // ######
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00011111, // #####
                    (byte) 0b00011111, // #####
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @1110 'c' (5 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00001110, //  ###
                    (byte) 0b00011110, //  ####
                    (byte) 0b00011011, // ## ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00011011, // ## ##
                    (byte) 0b00011110, //  ####
                    (byte) 0b00001110, //  ###
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @1125 'd' (6 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00110000, //      ##
                    (byte) 0b00110000, //      ##
                    (byte) 0b00110000, //      ##
                    (byte) 0b00111110, //  #####
                    (byte) 0b00111111, // ######
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00111111, // ######
                    (byte) 0b00111110, //  #####
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @1140 'e' (5 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00001110, //  ###
                    (byte) 0b00011111, // #####
                    (byte) 0b00011011, // ## ##
                    (byte) 0b00011111, // #####
                    (byte) 0b00000011, // ##
                    (byte) 0b00011011, // ## ##
                    (byte) 0b00011111, // #####
                    (byte) 0b00001110, //  ###
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @1155 'f' (4 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001110, //  ###
                    (byte) 0b00000110, //  ##
                    (byte) 0b00001111, // ####
                    (byte) 0b00001111, // ####
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @1170 'g' (6 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00110110, //  ## ##
                    (byte) 0b00111111, // ######
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00111111, // ######
                    (byte) 0b00110110, //  ## ##
                    (byte) 0b00110000, //      ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00011110, //  ####

                    // @1185 'h' (6 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00011011, // ## ##
                    (byte) 0b00111111, // ######
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @1200 'i' (2 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000000, //
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @1215 'j' (4 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00000000, //
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001111, // ####
                    (byte) 0b00000111, // ###

                    // @1230 'k' (6 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00011011, // ## ##
                    (byte) 0b00001111, // ####
                    (byte) 0b00011111, // #####
                    (byte) 0b00011011, // ## ##
                    (byte) 0b00011011, // ## ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @1245 'l' (2 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @1260 'm' (10 pixels wide)
                    (byte) 0b00000000, (byte) 0b00000000, //
                    (byte) 0b00000000, (byte) 0b00000000, //
                    (byte) 0b00000000, (byte) 0b00000000, //
                    (byte) 0b00000000, (byte) 0b00000000, //
                    (byte) 0b10011011, (byte) 0b00000001, // ## ##  ##
                    (byte) 0b11111111, (byte) 0b00000011, // ##########
                    (byte) 0b00110011, (byte) 0b00000011, // ##  ##  ##
                    (byte) 0b00110011, (byte) 0b00000011, // ##  ##  ##
                    (byte) 0b00110011, (byte) 0b00000011, // ##  ##  ##
                    (byte) 0b00110011, (byte) 0b00000011, // ##  ##  ##
                    (byte) 0b00110011, (byte) 0b00000011, // ##  ##  ##
                    (byte) 0b00110011, (byte) 0b00000011, // ##  ##  ##
                    (byte) 0b00000000, (byte) 0b00000000, //
                    (byte) 0b00000000, (byte) 0b00000000, //
                    (byte) 0b00000000, (byte) 0b00000000, //

                    // @1290 'n' (6 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00011011, // ## ##
                    (byte) 0b00111111, // ######
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @1305 'o' (6 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00011110, //  ####
                    (byte) 0b00011110, //  ####
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00011110, //  ####
                    (byte) 0b00011110, //  ####
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @1320 'p' (6 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00011011, // ## ##
                    (byte) 0b00111111, // ######
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00011111, // #####
                    (byte) 0b00011011, // ## ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##

                    // @1335 'q' (6 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00110110, //  ## ##
                    (byte) 0b00111111, // ######
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00111111, // ######
                    (byte) 0b00110110, //  ## ##
                    (byte) 0b00110000, //      ##
                    (byte) 0b00110000, //      ##
                    (byte) 0b00110000, //      ##

                    // @1350 'r' (4 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00001011, // ## #
                    (byte) 0b00001111, // ####
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @1365 's' (5 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00001110, //  ###
                    (byte) 0b00011111, // #####
                    (byte) 0b00010011, // ##  #
                    (byte) 0b00000111, // ###
                    (byte) 0b00011100, //   ###
                    (byte) 0b00011001, // #  ##
                    (byte) 0b00011111, // #####
                    (byte) 0b00001110, //  ###
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @1380 't' (4 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000100, //   #
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00001111, // ####
                    (byte) 0b00001111, // ####
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00001110, //  ###
                    (byte) 0b00001110, //  ###
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @1395 'u' (6 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00111111, // ######
                    (byte) 0b00110110, //  ## ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @1410 'v' (7 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b01100011, // ##   ##
                    (byte) 0b01100011, // ##   ##
                    (byte) 0b00110110, //  ## ##
                    (byte) 0b00110110, //  ## ##
                    (byte) 0b00110110, //  ## ##
                    (byte) 0b00011100, //   ###
                    (byte) 0b00011100, //   ###
                    (byte) 0b00011100, //   ###
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @1425 'w' (9 pixels wide)
                    (byte) 0b00000000, (byte) 0b00000000, //
                    (byte) 0b00000000, (byte) 0b00000000, //
                    (byte) 0b00000000, (byte) 0b00000000, //
                    (byte) 0b00000000, (byte) 0b00000000, //
                    (byte) 0b10111011, (byte) 0b00000001, // ## ### ##
                    (byte) 0b10111011, (byte) 0b00000001, // ## ### ##
                    (byte) 0b10111011, (byte) 0b00000001, // ## ### ##
                    (byte) 0b10101011, (byte) 0b00000001, // ## # # ##
                    (byte) 0b10101010, (byte) 0b00000000, //  # # # #
                    (byte) 0b11101110, (byte) 0b00000000, //  ### ###
                    (byte) 0b11101110, (byte) 0b00000000, //  ### ###
                    (byte) 0b11101110, (byte) 0b00000000, //  ### ###
                    (byte) 0b00000000, (byte) 0b00000000, //
                    (byte) 0b00000000, (byte) 0b00000000, //
                    (byte) 0b00000000, (byte) 0b00000000, //

                    // @1455 'x' (6 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00011110, //  ####
                    (byte) 0b00011110, //  ####
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00011110, //  ####
                    (byte) 0b00011110, //  ####
                    (byte) 0b00110011, // ##  ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @1470 'y' (7 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b01100011, // ##   ##
                    (byte) 0b01100011, // ##   ##
                    (byte) 0b00110110, //  ## ##
                    (byte) 0b00110110, //  ## ##
                    (byte) 0b00110110, //  ## ##
                    (byte) 0b00011100, //   ###
                    (byte) 0b00011100, //   ###
                    (byte) 0b00011100, //   ###
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001111, // ####
                    (byte) 0b00000111, // ###

                    // @1485 'z' (6 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00111111, // ######
                    (byte) 0b00111111, // ######
                    (byte) 0b00011000, //    ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00111111, // ######
                    (byte) 0b00111111, // ######
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @1500 '{' (4 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001110, //  ###
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00001110, //  ###
                    (byte) 0b00001100, //   ##

                    // @1515 '|' (2 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##
                    (byte) 0b00000011, // ##

                    // @1530 '}' (4 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000011, // ##
                    (byte) 0b00000111, // ###
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00001100, //   ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000111, // ###
                    (byte) 0b00000011, // ##

                    // @1545 '~' (6 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00100111, // ###  #
                    (byte) 0b00111111, // ######
                    (byte) 0b00111001, // #  ###
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @1560 '°' (5 pixels wide)
                    (byte) 0b00000000, //
                    (byte) 0b00001110, //  ###
                    (byte) 0b00010001, // #   #
                    (byte) 0b00010001, // #   #
                    (byte) 0b00010001, // #   #
                    (byte) 0b00001110, //  ###
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @1575 '²' (4 pixels wide)
                    (byte) 0b00000110, //  ##
                    (byte) 0b00001001, // #  #
                    (byte) 0b00001000, //    #
                    (byte) 0b00000100, //   #
                    (byte) 0b00000010, //  #
                    (byte) 0b00001111, // ####
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //

                    // @1590 '³' (4 pixels wide)
                    (byte) 0b00001110, //  ###
                    (byte) 0b00001001, // #  #
                    (byte) 0b00000100, //   #
                    (byte) 0b00001000, //    #
                    (byte) 0b00001001, // #  #
                    (byte) 0b00000110, //  ##
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
                    (byte) 0b00000000, //
            };
    public FontInfo fontInfo = new FontInfo('!', '³', 2, descriptors, bitmaps);
}
