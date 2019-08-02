update-alternatives --set llvm-config /usr/bin/llvm-config-$1
update-alternatives --set clang /usr/bin/clang-$1
update-alternatives --set opt /usr/bin/opt-$1
update-alternatives --set llvm-link /usr/bin/llvm-link-$1
rm -f /usr/local/lib/llvm
ln -s /usr/lib/llvm-$1 /usr/local/lib/llvm

