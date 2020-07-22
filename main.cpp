#include <iostream>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <fstream>
#include <sstream>
#include "colormod.hpp"

std::string exec(const char* cmd);
std::string parse_input(std::string);
std::string slurp(std::ifstream& in);

int main() {

  Color::Modifier red(Color::FG_RED);
  Color::Modifier cyan(Color::FG_CYAN);
  Color::Modifier green(Color::FG_GREEN);
  Color::Modifier def(Color::FG_DEFAULT);

  std::cout << red << "##########################################" << std::endl 
            << "#" << def << "             ARCH INSTALLER             " << red << "#" << std::endl
            << "##########################################" << std::endl;

  // Check EFI MODE
  std::cout << std::endl << cyan << "EFI mode check..." << std::endl;
  if(exec("ls /sys/firmware/efi/efivars").find("No such file or directory") != std::string::npos)
    std::cerr << std::endl << red << "!!! NOT IN EFI MODE !!!" << std::endl;
  std::cout << green << "You are in EFI mode" << std::endl;

  std::cout << std::endl << cyan << "I need some informations that are essential for the installation" << std::endl << def << std::endl;

  // PARTITIONS
  system("fdisk -l");

  std::string disk;
  std::cout << std::endl << cyan << "Select the disk where you want to install Arch Linux (example /dev/sda): " << def;
  std::getline(std::cin, disk);

  std::string boot_size = "+512M", swap_size = "+2G", root_size = "", input;
  std::cout << cyan << "Select the size for boot partition (default: +512M): " << def;
  boot_size = parse_input(boot_size);
  std::cout << cyan <<  "Select the size for swap partition (default: +2G): " << def;
  swap_size = parse_input(swap_size);
  std::cout << cyan << "Select the size for root partition (Rest): " << def;
  root_size = parse_input(root_size);

  // PACKAGES
  std::string packages = "base base-devel linux linux-firmware dhcpcd nano iwd networkmanager";
  std::string cpu;
  std::cout << std::endl << cyan << "Your cpu is: (1) Intel, (2) AMD: " << def;
  std::getline(std::cin, cpu);

  std::cout << std::endl << cyan << "Enter the packages you need separated by a space." << std::endl <<
                                    "(default: base base-devel linux linux-firmware dhcpcd nano vim iwd networkmanager):" << def << std::endl;
  packages = parse_input(packages);

  if(cpu == "1" && packages.find("intel-ucode") == std::string::npos)
    packages += " intel-ucode";
  else if(cpu == "2" && packages.find("amd-ucode") == std::string::npos)
    packages += " amd-ucode";

  // ZONEINFO
  std::string zoneinfo = "Europe/Rome";
  std::cout << std::endl << cyan << "Enter the zoneinfo (default: Europe/Rome): " << def;
  zoneinfo = parse_input(zoneinfo);

  // HOSTNAME
  std::string hostname = "arch";
  std::cout << std::endl << cyan << "Enter the hostname (default: arch): " << def;
  hostname = parse_input(hostname);

  // AUTO-INSTALLATION
  std::cout << std::endl << std::endl << red << "INSTALLATION..." << def << std::endl;
  system("timedatectl set-ntp true");

  std::ofstream fdisk_wrapper;
  fdisk_wrapper.open("fdisk.txt");

  std::string keystroke = "g\nn\n\n\n" + boot_size + "\nn\n\n\n" + swap_size + "\nn\n\n\n";
  root_size == "" ? keystroke += "\n" : keystroke += root_size + "\n";
  keystroke += "t\n1\n1\nt\n2\n19\nt\n3\n24\nw\n";

  fdisk_wrapper << keystroke;
  fdisk_wrapper.close();

  system(("fdisk " + disk + " < fdisk.txt").c_str());
  system(("mkfs.vfat " + disk + "1").c_str());
  system(("mkswap " + disk + "2").c_str());
  system(("swapon " + disk + "2").c_str());
  system(("mkfs.ext4 " + disk + "3").c_str());

  system(("mount " + disk + "3 /mnt").c_str());
  system("mkdir /mnt/boot");
  system(("mount " + disk + "1 /mnt/boot").c_str());

  system(("pacstrap /mnt " + packages).c_str());

  system("genfstab -U /mnt >> /mnt/etc/fstab");
  std::string fstab = exec("cat /mnt/etc/fstab");

  // CHROOT WRAPPER
  std::ofstream chroot_wrapper;
  chroot_wrapper.open("chroot.txt");
  keystroke = "ln -sf /usr/share/zoneinfo/" + zoneinfo + " /etc/localtime\nhwclock --systohc\nlocale-gen\nexit\n";
  chroot_wrapper << keystroke;
  chroot_wrapper.close();

  system("arch-chroot /mnt < chroot.txt");

  std::ifstream locale_gen;
  locale_gen.open("/mnt/etc/locale.gen");
  std::string temp_locale_gen = slurp(locale_gen);
  locale_gen.close();

  int pos = temp_locale_gen.find("#en_US.UTF-8 UTF-8", 0);
  temp_locale_gen.erase(pos, 1);

  std::ofstream new_locale_gen;
  new_locale_gen.open("/mnt/etc/locale.gen");
  new_locale_gen << temp_locale_gen;
  new_locale_gen.close();

  std::ofstream hosts;
  hosts.open("/mnt/etc/hosts");
  hosts << "127.0.0.1    localhost\n::1          localhost\n127.0.1.1    " + hostname + ".localdomain    " + hostname;
  hosts.close();

  chroot_wrapper.open("chroot.txt");
  keystroke = "echo 'LANG=en_US.UTF-8' > /etc/locale.conf\necho " + hostname + " > /etc/hostname\nbootctl --path=/boot install\nexit\n";
  chroot_wrapper << keystroke;
  chroot_wrapper.close();

  system("arch-chroot /mnt < chroot.txt");

  std::ofstream loader;
  loader.open("/mnt/boot/loader/loader.conf");
  loader << "default arch-*";
  loader.close();

  int uuid_pos = fstab.find(disk + "3");
  std::string uuid = fstab.substr(uuid_pos + disk.length() + 2, 41);

  std::ofstream arch;
  arch.open("/mnt/boot/loader/entries/arch.conf");
  std::string conf;
  cpu == "1" ? conf = "title   Arch Linux\nlinux /vmlinuz-linux\ninitrd /intel-ucode.img\ninitrd /initramfs-linux.img\noptions root=" + uuid + " rw" : conf = "title   Arch Linux\nlinux /vmlinuz-linux\ninitrd /amd-ucode.img\ninitrd /initramfs-linux.img\noptions root=" + uuid + " rw";
  arch << conf;
  arch.close();

  std::cout << std::endl << std::endl << red << "PASSWORD" << std::endl;
  std::cout << cyan << "Type \"passwd\" to set the password. Once done type \"exit\" and then \"reboot\" !" << def << std::endl;
  system("arch-chroot /mnt");

  return 0;
}

std::string exec(const char* cmd) {
  std::array<char, 128> buffer;
  std::string result;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);

  if(!pipe) {
    throw std::runtime_error("popen() failed!");
  }
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }

  return result;
}

std::string parse_input(std::string def) {
  std::string input = "";
  std::getline(std::cin, input);

  if(input.compare("") != 0) 
    return input;
  else
    return def;
}

std::string slurp(std::ifstream& in) {
  std::stringstream sstr;
  sstr << in.rdbuf();
  return sstr.str();
}
