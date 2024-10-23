import os
import time
import signal
import subprocess
from multiprocessing import Process, Queue


def run_qemu_command(queue, iteration, wait_time, expected_out):
    run_command = [
        "qemu-system-aarch64",
        "-machine",
        "virt,gic-version=3",
        "-cpu",
        "cortex-a72",
        "-smp",
        "4",
        "-m",
        "4096",
        "-nographic",
        "-monitor",
        "none",
        "-serial",
        "mon:stdio",
        "-global",
        "virtio-mmio.force-legacy=false",
        "-kernel",
        "/root/data/OS-2024/build/src/kernel8.elf",
    ]

    # 执行QEMU, 并等待一段时间
    process = subprocess.Popen(run_command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(wait_time)

    # 退出QEMU, 并获取输出
    os.kill(process.pid, signal.SIGINT)
    stdout, stderr = process.communicate()

    # 检查测试是否成功
    if process.returncode == 0:
        if expected_out in stdout.decode():
            result = f"# {iteration + 1}:pass"
        else:
            result = f"# {iteration + 1}:fail\n Output:\n{stdout.decode()}"

    # 将结果放入队列
    queue.put(result)


def main():
    wait_time = 2  # 每批次的等待时间
    batch_size = 5  # 每批次的测试数
    batch_num = 100  # 总批次数
    expected_out = "proc_test PASS"
    os.chdir("/root/data/OS-2024/build")

    # 首先执行make命令编译内核
    build_command = ["make", "qemu-build"]
    subprocess.Popen(build_command).wait()

    processes = []
    queue = Queue()

    error = False
    print(f"wait_time={wait_time}s, batch_size={batch_size}, batch_num={batch_num}")
    for batch in range(batch_num):
        print(f"Start Batch {batch + 1}:")
        for i in range(batch * batch_size, (batch + 1) * batch_size):
            p = Process(target=run_qemu_command, args=(queue, i, wait_time, expected_out))
            processes.append(p)
            p.start()

        for p in processes:
            p.join()

        processes.clear()

        lines = queue.get().split("#")
        for line in lines[1:]:
            if line.split(":")[1].strip() != "pass":
                error = True
                print(line)

    if not error:
        print("All tests passed!")


if __name__ == "__main__":
    main()
