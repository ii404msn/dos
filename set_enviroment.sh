sudo apt-get install build-essential
mv ~/.vim ~/.vim.orig
mv ~/.vimrc ~/.vimrc.orig
git clone git://github.com/fuhu/.vimrc.git ~/.vim
ln -s ~/.vim/vimrc ~/.vimrc
git clone https://github.com/gmarik/Vundle.vim.git ~/.vim/bundle/Vundle.vim
