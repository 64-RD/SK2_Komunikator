import PySimpleGUI as sg
import socket
from time import sleep


def main():

    # GUI login window.
    layout = [
        [sg.Text('Enter username and password.')],
        [sg.Text('Username:', size=(15, 1)), sg.InputText()],
        [sg.Text('Password:', size=(15, 1)), sg.InputText(password_char='*')],
        [sg.Submit(), sg.Cancel()]
    ]
    window = sg.Window('SK2_Komunikator', layout)

    # Connection start
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(('192.168.1.64', 1124))

    while True:

        event, values = window.read()

        s.send(list(values.values())[0].encode())
        sleep(1)
        s.send(list(values.values())[1].encode())

        data = s.recv(1024).decode().strip()
        print(len(data), data)
        sg.popup(data)

        if data[6] == "s":
            window.close()
            break

    '''
    # Event Loop to process "events" and get the "values" of the inputs
    while True:
        event, values = window.read()
        if event == sg.WIN_CLOSED or event == 'sss':  # if user closes window or clicks cancel
            break

    window.close()
    return
    '''


if __name__ == "__main__":
    main()
