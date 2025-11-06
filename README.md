# Zybo Memcopy Accelerator (Interrupt-Based)

### Design and Implementation of a Memory Copy Accelerator on Zybo Z7-10
**Author:** Van Dinh Tran  
**Board:** Zybo Z7-10  
**Tools:** Vivado / Vitis 2022.1  
**Language:** C / HLS C++  
*Note: This guide also work with the Zedboard Zynq-7000*
---

## 🧩 Overview
This project demonstrates a hardware-accelerated memory copy (memcpy) function designed using **Vivado HLS**, integrated with **Zynq PS** on the **Zybo Z7-10** board.

The accelerator operates using an **interrupt-driven mechanism**, ensuring synchronization between PS (ARM Cortex-A9) and PL (FPGA fabric).

---

## 📁 Project Structure



